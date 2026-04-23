#define WEBVIEW_HEADER
#include "webview.h"

#include <windows.h>

#include "WebView2.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifndef WC_ERR_INVALID_CHARS
#define WC_ERR_INVALID_CHARS 0x00000080
#endif

namespace {

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

static std::wstring widen_string(const char *input) {
  if (input == nullptr || *input == '\0') {
    return std::wstring();
  }
  int required =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, nullptr, 0);
  if (required <= 0) {
    return std::wstring();
  }
  std::wstring output(static_cast<std::size_t>(required), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, &output[0],
                      required);
  output.resize(static_cast<std::size_t>(required - 1));
  return output;
}

static std::string narrow_string(const wchar_t *input) {
  if (input == nullptr || *input == L'\0') {
    return std::string();
  }
  int required =
      WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input, -1, nullptr, 0,
                          nullptr, nullptr);
  if (required <= 0) {
    return std::string();
  }
  std::string output(static_cast<std::size_t>(required), '\0');
  WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input, -1, &output[0],
                      required, nullptr, nullptr);
  output.resize(static_cast<std::size_t>(required - 1));
  return output;
}

static std::string trim_system_message(std::string message) {
  while (!message.empty() &&
         (message.back() == '\r' || message.back() == '\n' || message.back() == ' ')) {
    message.pop_back();
  }
  return message;
}

static char *copy_hresult_error(HRESULT hr, const char *context) {
  LPWSTR buffer = nullptr;
  DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD len = FormatMessageW(flags, nullptr, static_cast<DWORD>(hr),
                             MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                             reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);

  std::string message = context ? std::string(context) : std::string("cookie manager error");
  message += ": ";
  if (len > 0 && buffer != nullptr) {
    message += trim_system_message(narrow_string(buffer));
    LocalFree(buffer);
  } else {
    char fallback[32];
    std::snprintf(fallback, sizeof(fallback), "HRESULT 0x%08lx",
                  static_cast<unsigned long>(hr));
    message += fallback;
  }
  return copy_string(message);
}

static std::string json_escape(const std::string &text) {
  std::string out = "\"";
  for (unsigned char ch : text) {
    switch (ch) {
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
      if (ch < 0x20) {
        static const char hex[] = "0123456789abcdef";
        out += "\\u00";
        out += hex[(ch >> 4) & 0x0f];
        out += hex[ch & 0x0f];
      } else {
        out += static_cast<char>(ch);
      }
    }
  }
  out += "\"";
  return out;
}

static HRESULT get_cookie_manager(webview_t w,
                                  ICoreWebView2CookieManager **manager) {
  if (manager == nullptr) {
    return E_POINTER;
  }
  *manager = nullptr;

  auto *controller = static_cast<ICoreWebView2Controller *>(
      webview_get_native_handle(w, WEBVIEW_NATIVE_HANDLE_KIND_BROWSER_CONTROLLER));
  if (controller == nullptr) {
    return E_POINTER;
  }

  ICoreWebView2 *webview_base = nullptr;
  HRESULT hr = controller->get_CoreWebView2(&webview_base);
  if (FAILED(hr)) {
    return hr;
  }

  ICoreWebView2_2 *webview = nullptr;
  hr = webview_base->QueryInterface(IID_ICoreWebView2_2,
                                    reinterpret_cast<void **>(&webview));
  webview_base->Release();
  if (FAILED(hr)) {
    return hr;
  }
  hr = webview->get_CookieManager(manager);
  webview->Release();
  return hr;
}

static std::string json_for_cookie(ICoreWebView2Cookie *cookie);

class get_cookies_request {
public:
  explicit get_cookies_request(const char *url)
      : m_event(CreateEventW(nullptr, TRUE, FALSE, nullptr)),
        m_url(widen_string(url)) {}

  ~get_cookies_request() {
    if (m_json != nullptr) {
      free(m_json);
      m_json = nullptr;
    }
    if (m_error != nullptr) {
      free(m_error);
      m_error = nullptr;
    }
    if (m_event != nullptr) {
      CloseHandle(m_event);
      m_event = nullptr;
    }
  }

  void AddRef() { InterlockedIncrement(&m_ref_count); }

  void Release() {
    if (InterlockedDecrement(&m_ref_count) == 0) {
      delete this;
    }
  }

  HANDLE event_handle() const { return m_event; }

  const wchar_t *url() const { return m_url.c_str(); }

  void set_json(const std::string &payload) {
    if (m_json == nullptr) {
      m_json = copy_string(payload);
    }
    signal();
  }

  void set_error(char *message) {
    if (m_error == nullptr) {
      m_error = message;
    } else if (message != nullptr) {
      free(message);
    }
    signal();
  }

  char *take_json() {
    char *value = m_json;
    m_json = nullptr;
    return value;
  }

  char *take_error() {
    char *value = m_error;
    m_error = nullptr;
    return value;
  }

private:
  void signal() {
    if (m_event != nullptr) {
      SetEvent(m_event);
    }
  }

  LONG m_ref_count = 1;
  HANDLE m_event = nullptr;
  std::wstring m_url;
  char *m_json = nullptr;
  char *m_error = nullptr;
};

static HRESULT serialize_cookie_list(ICoreWebView2CookieList *cookie_list,
                                     std::string *payload) {
  if (payload == nullptr) {
    return E_POINTER;
  }
  *payload = "[";
  UINT count = 0;
  bool first = true;
  if (cookie_list != nullptr) {
    HRESULT hr = cookie_list->get_Count(&count);
    if (FAILED(hr)) {
      return hr;
    }
    for (UINT i = 0; i < count; ++i) {
      ICoreWebView2Cookie *cookie = nullptr;
      hr = cookie_list->GetValueAtIndex(i, &cookie);
      if (FAILED(hr) || cookie == nullptr) {
        continue;
      }
      if (!first) {
        *payload += ",";
      }
      first = false;
      *payload += json_for_cookie(cookie);
      cookie->Release();
    }
  }
  *payload += "]";
  return S_OK;
}

class get_cookies_handler : public ICoreWebView2GetCookiesCompletedHandler {
public:
  explicit get_cookies_handler(get_cookies_request *request) : m_request(request) {
    if (m_request != nullptr) {
      m_request->AddRef();
    }
  }

  ~get_cookies_handler() {
    if (m_request != nullptr) {
      m_request->Release();
      m_request = nullptr;
    }
  }

  ULONG STDMETHODCALLTYPE AddRef() override {
    return static_cast<ULONG>(InterlockedIncrement(&m_ref_count));
  }

  ULONG STDMETHODCALLTYPE Release() override {
    auto value = InterlockedDecrement(&m_ref_count);
    if (value == 0) {
      delete this;
      return 0;
    }
    return static_cast<ULONG>(value);
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override {
    if (ppvObject == nullptr) {
      return E_POINTER;
    }
    *ppvObject = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ICoreWebView2GetCookiesCompletedHandler)) {
      *ppvObject =
          static_cast<ICoreWebView2GetCookiesCompletedHandler *>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE Invoke(HRESULT result,
                                   ICoreWebView2CookieList *cookie_list) override {
    if (m_request == nullptr) {
      return S_OK;
    }
    if (FAILED(result)) {
      m_request->set_error(copy_hresult_error(result, "cookie callback failed"));
      return S_OK;
    }

    std::string payload;
    HRESULT hr = serialize_cookie_list(cookie_list, &payload);
    if (FAILED(hr)) {
      m_request->set_error(copy_hresult_error(hr, "failed to serialize cookies"));
      return S_OK;
    }
    m_request->set_json(payload);
    return S_OK;
  }

private:
  LONG m_ref_count = 1;
  get_cookies_request *m_request = nullptr;
};

static void start_get_cookies_request(webview_t w, void *arg) {
  auto *request = static_cast<get_cookies_request *>(arg);
  if (request == nullptr) {
    return;
  }

  ICoreWebView2CookieManager *manager = nullptr;
  HRESULT hr = get_cookie_manager(w, &manager);
  if (FAILED(hr)) {
    request->set_error(
        copy_hresult_error(hr, "failed to resolve ICoreWebView2CookieManager"));
    request->Release();
    return;
  }

  auto *handler = new get_cookies_handler(request);
  hr = manager->GetCookies(request->url(), handler);
  manager->Release();
  handler->Release();
  if (FAILED(hr)) {
    request->set_error(copy_hresult_error(hr, "GetCookies failed"));
  }
  request->Release();
}

static bool wait_for_event_with_messages(HANDLE event_handle, DWORD timeout_ms) {
  DWORD start = GetTickCount();
  bool repost_quit = false;
  int quit_code = 0;

  while (true) {
    DWORD elapsed = GetTickCount() - start;
    if (elapsed >= timeout_ms) {
      if (repost_quit) {
        PostQuitMessage(quit_code);
      }
      return false;
    }
    DWORD wait_ms = timeout_ms - elapsed;
    DWORD wait_result =
        MsgWaitForMultipleObjects(1, &event_handle, FALSE, wait_ms, QS_ALLINPUT);
    if (wait_result == WAIT_OBJECT_0) {
      if (repost_quit) {
        PostQuitMessage(quit_code);
      }
      return true;
    }
    if (wait_result == WAIT_TIMEOUT) {
      if (repost_quit) {
        PostQuitMessage(quit_code);
      }
      return false;
    }
    if (wait_result == WAIT_OBJECT_0 + 1) {
      MSG msg;
      while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
          repost_quit = true;
          quit_code = static_cast<int>(msg.wParam);
          continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
      continue;
    }
    if (repost_quit) {
      PostQuitMessage(quit_code);
    }
    return false;
  }
}

static std::string json_for_cookie(ICoreWebView2Cookie *cookie) {
  LPWSTR name = nullptr;
  LPWSTR value = nullptr;
  LPWSTR domain = nullptr;
  LPWSTR path = nullptr;
  BOOL http_only = FALSE;
  BOOL secure = FALSE;
  BOOL is_session = FALSE;
  double expires = -1.0;

  cookie->get_Name(&name);
  cookie->get_Value(&value);
  cookie->get_Domain(&domain);
  cookie->get_Path(&path);
  cookie->get_IsHttpOnly(&http_only);
  cookie->get_IsSecure(&secure);
  cookie->get_IsSession(&is_session);
  if (!is_session) {
    cookie->get_Expires(&expires);
  }

  std::string json = "{";
  json += "\"name\":" + json_escape(narrow_string(name)) + ",";
  json += "\"value\":" + json_escape(narrow_string(value)) + ",";
  json += "\"domain\":" + json_escape(narrow_string(domain)) + ",";
  json += "\"path\":" + json_escape(narrow_string(path)) + ",";
  if (!is_session && expires >= 0) {
    json += "\"expires_unix\":" + std::to_string(static_cast<long long>(expires)) + ",";
  }
  json += "\"secure\":";
  json += secure ? "true" : "false";
  json += ",";
  json += "\"http_only\":";
  json += http_only ? "true" : "false";
  json += "}";

  if (name != nullptr) {
    CoTaskMemFree(name);
  }
  if (value != nullptr) {
    CoTaskMemFree(value);
  }
  if (domain != nullptr) {
    CoTaskMemFree(domain);
  }
  if (path != nullptr) {
    CoTaskMemFree(path);
  }
  return json;
}

} // namespace

extern "C" char *CgoWebViewGetCookies(webview_t w, const char *url, char **err) {
  if (err != nullptr) {
    *err = nullptr;
  }

  auto *request = new get_cookies_request(url);
  if (request->event_handle() == nullptr) {
    request->Release();
    if (err != nullptr) {
      *err = copy_error("failed to create cookie wait event");
    }
    return nullptr;
  }

  request->AddRef();
  webview_dispatch(w, start_get_cookies_request, request);

  auto hwnd = static_cast<HWND>(
      webview_get_native_handle(w, WEBVIEW_NATIVE_HANDLE_KIND_UI_WINDOW));
  DWORD ui_thread_id = hwnd ? GetWindowThreadProcessId(hwnd, nullptr) : 0;
  bool completed = false;
  if (ui_thread_id != 0 && ui_thread_id == GetCurrentThreadId()) {
    completed = wait_for_event_with_messages(request->event_handle(), 5000);
  } else {
    completed = WaitForSingleObject(request->event_handle(), 5000) == WAIT_OBJECT_0;
  }

  char *result = request->take_json();
  char *request_error = request->take_error();
  request->Release();

  if (!completed) {
    if (result != nullptr) {
      free(result);
    }
    if (request_error != nullptr) {
      free(request_error);
    }
    if (err != nullptr) {
      *err = copy_error("timed out while reading cookies");
    }
    return nullptr;
  }

  if (request_error != nullptr) {
    if (result != nullptr) {
      free(result);
    }
    if (err != nullptr) {
      *err = request_error;
    } else {
      free(request_error);
    }
    return nullptr;
  }

  return result;
}

extern "C" char *CgoWebViewSetCookie(webview_t w, const char *name,
                                     const char *value, const char *domain,
                                     const char *path, int http_only,
                                     int secure, int has_expires,
                                     double expires_unix, char **err) {
  if (err != nullptr) {
    *err = nullptr;
  }

  ICoreWebView2CookieManager *manager = nullptr;
  HRESULT hr = get_cookie_manager(w, &manager);
  if (FAILED(hr)) {
    if (err != nullptr) {
      *err = copy_hresult_error(hr, "failed to resolve ICoreWebView2CookieManager");
    }
    return nullptr;
  }

  std::wstring wname = widen_string(name);
  std::wstring wvalue = widen_string(value);
  std::wstring wdomain = widen_string(domain);
  std::wstring wpath = widen_string(path);

  ICoreWebView2Cookie *cookie = nullptr;
  hr = manager->CreateCookie(wname.c_str(), wvalue.c_str(), wdomain.c_str(),
                             wpath.c_str(), &cookie);
  if (FAILED(hr) || cookie == nullptr) {
    manager->Release();
    if (err != nullptr) {
      *err = copy_hresult_error(hr, "CreateCookie failed");
    }
    return nullptr;
  }

  cookie->put_IsHttpOnly(http_only ? TRUE : FALSE);
  cookie->put_IsSecure(secure ? TRUE : FALSE);
  if (has_expires) {
    cookie->put_Expires(expires_unix);
  }

  hr = manager->AddOrUpdateCookie(cookie);
  cookie->Release();
  manager->Release();
  if (FAILED(hr) && err != nullptr) {
    *err = copy_hresult_error(hr, "AddOrUpdateCookie failed");
  }
  return nullptr;
}

extern "C" char *CgoWebViewDeleteCookie(webview_t w, const char *name,
                                        const char *domain, const char *path) {
  ICoreWebView2CookieManager *manager = nullptr;
  HRESULT hr = get_cookie_manager(w, &manager);
  if (FAILED(hr)) {
    return copy_hresult_error(hr, "failed to resolve ICoreWebView2CookieManager");
  }

  std::wstring wname = widen_string(name);
  std::wstring wdomain = widen_string(domain);
  std::wstring wpath = widen_string(path);
  hr = manager->DeleteCookiesWithDomainAndPath(wname.c_str(), wdomain.c_str(),
                                               wpath.c_str());
  manager->Release();
  if (FAILED(hr)) {
    return copy_hresult_error(hr, "DeleteCookiesWithDomainAndPath failed");
  }
  return nullptr;
}

extern "C" char *CgoWebViewClearCookies(webview_t w) {
  ICoreWebView2CookieManager *manager = nullptr;
  HRESULT hr = get_cookie_manager(w, &manager);
  if (FAILED(hr)) {
    return copy_hresult_error(hr, "failed to resolve ICoreWebView2CookieManager");
  }
  hr = manager->DeleteAllCookies();
  manager->Release();
  if (FAILED(hr)) {
    return copy_hresult_error(hr, "DeleteAllCookies failed");
  }
  return nullptr;
}

extern "C" void CgoWebViewFreeString(char *s) { free(s); }
