#define WEBVIEW_HEADER
#include "webview.h"

#include <glib.h>
#include <libsoup/soup.h>
#include <webkit2/webkit2.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>

namespace {

struct async_result {
  gboolean done = FALSE;
  gboolean ok = FALSE;
  GError *error = nullptr;
  GList *cookies = nullptr;
};

static char *copy_string(const std::string &value) {
  char *result = static_cast<char *>(malloc(value.size() + 1));
  if (result == nullptr) {
    return nullptr;
  }
  memcpy(result, value.c_str(), value.size() + 1);
  return result;
}

static char *copy_error(const char *message) {
  if (message == nullptr) {
    message = "unknown cookie manager error";
  }
  return copy_string(message);
}

static char *copy_gerror(GError *error) {
  if (error == nullptr) {
    return nullptr;
  }
  std::string message = error->message ? error->message : "unknown cookie manager error";
  g_error_free(error);
  return copy_string(message);
}

static std::string json_escape(const char *text) {
  std::string out = "\"";
  if (text != nullptr) {
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p; ++p) {
      switch (*p) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (*p < 0x20) {
          static const char hex[] = "0123456789abcdef";
          out += "\\u00";
          out += hex[(*p >> 4) & 0x0f];
          out += hex[*p & 0x0f];
        } else {
          out += static_cast<char>(*p);
        }
      }
    }
  }
  out += "\"";
  return out;
}

static gboolean wait_until_done(async_result *result, gint64 timeout_us) {
  GMainContext *context = g_main_context_default();
  gint64 deadline = g_get_monotonic_time() + timeout_us;
  while (!result->done) {
    if (g_get_monotonic_time() >= deadline) {
      return FALSE;
    }
    while (g_main_context_pending(context)) {
      g_main_context_iteration(context, FALSE);
    }
    g_usleep(1000);
  }
  return TRUE;
}

static WebKitCookieManager *cookie_manager_for_webview(webview_t w) {
  auto *webview = WEBKIT_WEB_VIEW(
      webview_get_native_handle(w, WEBVIEW_NATIVE_HANDLE_KIND_BROWSER_CONTROLLER));
  if (webview == nullptr) {
    return nullptr;
  }
  auto *context = webkit_web_view_get_context(webview);
  if (context == nullptr) {
    return nullptr;
  }
  return webkit_web_context_get_cookie_manager(context);
}

static void on_get_cookies_finished(GObject *source_object, GAsyncResult *res,
                                    gpointer user_data) {
  auto *result = static_cast<async_result *>(user_data);
  result->cookies = webkit_cookie_manager_get_cookies_finish(
      WEBKIT_COOKIE_MANAGER(source_object), res, &result->error);
  result->done = TRUE;
}

static void on_add_cookie_finished(GObject *source_object, GAsyncResult *res,
                                   gpointer user_data) {
  auto *result = static_cast<async_result *>(user_data);
  auto *manager = WEBKIT_COOKIE_MANAGER(source_object);
  result->ok = webkit_cookie_manager_add_cookie_finish(manager, res, &result->error);
  result->done = TRUE;
}

static void on_delete_cookie_finished(GObject *source_object, GAsyncResult *res,
                                      gpointer user_data) {
  auto *result = static_cast<async_result *>(user_data);
  auto *manager = WEBKIT_COOKIE_MANAGER(source_object);
  result->ok = webkit_cookie_manager_delete_cookie_finish(manager, res, &result->error);
  result->done = TRUE;
}

static int64_t cookie_expires_unix(SoupCookie *cookie, gboolean *has_expires) {
#if SOUP_MAJOR_VERSION >= 3
  GDateTime *expires = soup_cookie_get_expires(cookie);
  if (expires == nullptr) {
    *has_expires = FALSE;
    return 0;
  }
  *has_expires = TRUE;
  return g_date_time_to_unix(expires);
#else
  SoupDate *expires = soup_cookie_get_expires(cookie);
  if (expires == nullptr) {
    *has_expires = FALSE;
    return 0;
  }
  *has_expires = TRUE;
  return soup_date_to_time_t(expires);
#endif
}

} // namespace

extern "C" char *CgoWebViewGetCookies(webview_t w, const char *url, char **err) {
  if (err != nullptr) {
    *err = nullptr;
  }
  auto *manager = cookie_manager_for_webview(w);
  if (manager == nullptr) {
    if (err != nullptr) {
      *err = copy_error("failed to resolve WebKitCookieManager");
    }
    return nullptr;
  }

  async_result result;
  webkit_cookie_manager_get_cookies(manager, url, nullptr, on_get_cookies_finished,
                                    &result);
  if (!wait_until_done(&result, 5 * G_USEC_PER_SEC)) {
    if (err != nullptr) {
      *err = copy_error("timed out while reading cookies");
    }
    return nullptr;
  }
  if (result.error != nullptr) {
    if (err != nullptr) {
      *err = copy_gerror(result.error);
    }
    if (result.cookies != nullptr) {
      g_list_free_full(result.cookies, reinterpret_cast<GDestroyNotify>(soup_cookie_free));
    }
    return nullptr;
  }

  std::string payload = "[";
  bool first = true;
  for (GList *node = result.cookies; node != nullptr; node = node->next) {
    auto *cookie = static_cast<SoupCookie *>(node->data);
    if (!first) {
      payload += ",";
    }
    first = false;

    gboolean has_expires = FALSE;
    int64_t expires_unix = cookie_expires_unix(cookie, &has_expires);

    payload += "{";
    payload += "\"name\":" + json_escape(soup_cookie_get_name(cookie)) + ",";
    payload += "\"value\":" + json_escape(soup_cookie_get_value(cookie)) + ",";
    payload += "\"domain\":" + json_escape(soup_cookie_get_domain(cookie)) + ",";
    payload += "\"path\":" + json_escape(soup_cookie_get_path(cookie)) + ",";
    if (has_expires) {
      payload += "\"expires_unix\":" + std::to_string(expires_unix) + ",";
    }
    payload += "\"secure\":";
    payload += soup_cookie_get_secure(cookie) ? "true" : "false";
    payload += ",";
    payload += "\"http_only\":";
    payload += soup_cookie_get_http_only(cookie) ? "true" : "false";
    payload += "}";
  }
  payload += "]";

  if (result.cookies != nullptr) {
    g_list_free_full(result.cookies, reinterpret_cast<GDestroyNotify>(soup_cookie_free));
  }
  return copy_string(payload);
}

extern "C" char *CgoWebViewSetCookie(webview_t w, const char *name,
                                     const char *value, const char *domain,
                                     const char *path, int http_only,
                                     int secure, int has_expires,
                                     double expires_unix, char **err) {
  if (err != nullptr) {
    *err = nullptr;
  }
  auto *manager = cookie_manager_for_webview(w);
  if (manager == nullptr) {
    if (err != nullptr) {
      *err = copy_error("failed to resolve WebKitCookieManager");
    }
    return nullptr;
  }

  SoupCookie *cookie = soup_cookie_new(name, value, domain, path, -1);
  if (cookie == nullptr) {
    if (err != nullptr) {
      *err = copy_error("failed to construct cookie");
    }
    return nullptr;
  }
  soup_cookie_set_http_only(cookie, http_only != 0);
  soup_cookie_set_secure(cookie, secure != 0);
  if (has_expires) {
#if SOUP_MAJOR_VERSION >= 3
    GDateTime *expires = g_date_time_new_from_unix_utc(static_cast<gint64>(expires_unix));
    soup_cookie_set_expires(cookie, expires);
    g_date_time_unref(expires);
#else
    SoupDate *expires = soup_date_new_from_time_t(static_cast<time_t>(expires_unix));
    soup_cookie_set_expires(cookie, expires);
    soup_date_free(expires);
#endif
  }

  async_result result;
  webkit_cookie_manager_add_cookie(manager, cookie, nullptr, on_add_cookie_finished,
                                   &result);
  if (!wait_until_done(&result, 5 * G_USEC_PER_SEC)) {
    soup_cookie_free(cookie);
    if (err != nullptr) {
      *err = copy_error("timed out while setting cookie");
    }
    return nullptr;
  }
  soup_cookie_free(cookie);
  if (result.error != nullptr) {
    if (err != nullptr) {
      *err = copy_gerror(result.error);
    }
    return nullptr;
  }
  if (!result.ok && err != nullptr) {
    *err = copy_error("failed to set cookie");
  }
  return nullptr;
}

extern "C" char *CgoWebViewDeleteCookie(webview_t w, const char *name,
                                        const char *domain, const char *path) {
  auto *manager = cookie_manager_for_webview(w);
  if (manager == nullptr) {
    return copy_error("failed to resolve WebKitCookieManager");
  }

  SoupCookie *cookie = soup_cookie_new(name, "", domain, path, -1);
  if (cookie == nullptr) {
    return copy_error("failed to construct cookie");
  }

  async_result result;
  webkit_cookie_manager_delete_cookie(manager, cookie, nullptr,
                                      on_delete_cookie_finished, &result);
  if (!wait_until_done(&result, 5 * G_USEC_PER_SEC)) {
    soup_cookie_free(cookie);
    return copy_error("timed out while deleting cookie");
  }
  soup_cookie_free(cookie);
  if (result.error != nullptr) {
    return copy_gerror(result.error);
  }
  if (!result.ok) {
    return copy_error("failed to delete cookie");
  }
  return nullptr;
}

extern "C" char *CgoWebViewClearCookies(webview_t w) {
  auto *manager = cookie_manager_for_webview(w);
  if (manager == nullptr) {
    return copy_error("failed to resolve WebKitCookieManager");
  }
  webkit_cookie_manager_delete_all_cookies(manager);
  return nullptr;
}

extern "C" void CgoWebViewFreeString(char *s) { free(s); }
