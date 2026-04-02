#define WEBVIEW_HEADER
#include "webview.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#include <stdlib.h>
#include <string.h>

static char *copy_error(NSString *message) {
  const char *utf8 = [message UTF8String];
  if (utf8 == nullptr) {
    utf8 = "unknown cookie manager error";
  }
  size_t len = strlen(utf8);
  char *result = static_cast<char *>(malloc(len + 1));
  if (result == nullptr) {
    return nullptr;
  }
  memcpy(result, utf8, len + 1);
  return result;
}

static char *copy_json(NSData *data) {
  if (data == nil) {
    return nullptr;
  }
  NSString *json =
      [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
  if (json == nil) {
    return copy_error(@"failed to encode cookie payload");
  }
  char *result = copy_error(json);
  [json release];
  return result;
}

static BOOL wait_until_done(BOOL *done, NSTimeInterval timeout_seconds) {
  NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:timeout_seconds];
  while (!*done) {
    if ([deadline timeIntervalSinceNow] <= 0) {
      return NO;
    }
    @autoreleasepool {
      [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                               beforeDate:[NSDate
                                              dateWithTimeIntervalSinceNow:0.01]];
    }
  }
  return YES;
}

static WKHTTPCookieStore *cookie_store_for_webview(webview_t w) {
  auto *handle = static_cast<WKWebView *>(
      webview_get_native_handle(w, WEBVIEW_NATIVE_HANDLE_KIND_BROWSER_CONTROLLER));
  if (handle == nil) {
    return nil;
  }
  return handle.configuration.websiteDataStore.httpCookieStore;
}

static BOOL cookie_matches_url(NSHTTPCookie *cookie, NSURL *url) {
  NSString *host = [[url host] lowercaseString];
  if (host == nil || [host length] == 0) {
    return NO;
  }

  NSString *domain = [[cookie domain] lowercaseString];
  if (domain == nil || [domain length] == 0) {
    return NO;
  }

  BOOL host_matches = [host isEqualToString:domain];
  if (!host_matches) {
    NSString *trimmed =
        [domain hasPrefix:@"."] ? [domain substringFromIndex:1] : domain;
    NSString *suffix = [@"." stringByAppendingString:trimmed];
    host_matches = [host isEqualToString:trimmed] || [host hasSuffix:suffix];
  }
  if (!host_matches) {
    return NO;
  }

  NSString *request_path = [url path];
  if (request_path == nil || [request_path length] == 0) {
    request_path = @"/";
  }
  NSString *cookie_path = [cookie path];
  if (cookie_path == nil || [cookie_path length] == 0) {
    cookie_path = @"/";
  }
  if (![request_path hasPrefix:cookie_path]) {
    return NO;
  }

  if ([cookie isSecure]) {
    NSString *scheme = [[url scheme] lowercaseString];
    BOOL secure_scheme =
        [scheme isEqualToString:@"https"] || [scheme isEqualToString:@"wss"];
    if (!secure_scheme) {
      return NO;
    }
  }

  return YES;
}

extern "C" char *CgoWebViewGetCookies(webview_t w, const char *url_cstr,
                                      char **err) {
  @autoreleasepool {
    if (err != nullptr) {
      *err = nullptr;
    }
    if (![NSThread isMainThread]) {
      if (err != nullptr) {
        *err = copy_error(@"cookie APIs must be called on the UI thread");
      }
      return nullptr;
    }

    NSURL *url = [NSURL URLWithString:[NSString stringWithUTF8String:url_cstr]];
    if (url == nil) {
      if (err != nullptr) {
        *err = copy_error(@"invalid cookie URL");
      }
      return nullptr;
    }

    WKHTTPCookieStore *store = cookie_store_for_webview(w);
    if (store == nil) {
      if (err != nullptr) {
        *err = copy_error(@"failed to resolve WKHTTPCookieStore");
      }
      return nullptr;
    }

    __block BOOL done = NO;
    __block NSArray<NSHTTPCookie *> *cookies = nil;
    [store getAllCookies:^(NSArray<NSHTTPCookie *> *all_cookies) {
      cookies = all_cookies;
      done = YES;
    }];
    if (!wait_until_done(&done, 5.0)) {
      if (err != nullptr) {
        *err = copy_error(@"timed out while reading cookies");
      }
      return nullptr;
    }

    NSMutableArray *payload = [NSMutableArray array];
    for (NSHTTPCookie *cookie in cookies) {
      if (!cookie_matches_url(cookie, url)) {
        continue;
      }
      NSMutableDictionary *item = [NSMutableDictionary dictionary];
      item[@"name"] = cookie.name ?: @"";
      item[@"value"] = cookie.value ?: @"";
      item[@"domain"] = cookie.domain ?: @"";
      item[@"path"] = cookie.path ?: @"/";
      item[@"secure"] = @(cookie.secure);
      item[@"http_only"] = @(cookie.HTTPOnly);
      if (cookie.expiresDate != nil) {
        item[@"expires_unix"] =
            @((long long)[cookie.expiresDate timeIntervalSince1970]);
      }
      [payload addObject:item];
    }

    NSError *json_error = nil;
    NSData *json_data =
        [NSJSONSerialization dataWithJSONObject:payload
                                        options:0
                                          error:&json_error];
    if (json_data == nil) {
      if (err != nullptr) {
        *err = copy_error(json_error.localizedDescription
                              ?: @"failed to serialize cookies");
      }
      return nullptr;
    }
    return copy_json(json_data);
  }
}

extern "C" char *CgoWebViewSetCookie(webview_t w, const char *name,
                                     const char *value, const char *domain,
                                     const char *path, int http_only,
                                     int secure, int has_expires,
                                     double expires_unix, char **err) {
  @autoreleasepool {
    if (err != nullptr) {
      *err = nullptr;
    }
    if (![NSThread isMainThread]) {
      if (err != nullptr) {
        *err = copy_error(@"cookie APIs must be called on the UI thread");
      }
      return nullptr;
    }

    WKHTTPCookieStore *store = cookie_store_for_webview(w);
    if (store == nil) {
      if (err != nullptr) {
        *err = copy_error(@"failed to resolve WKHTTPCookieStore");
      }
      return nullptr;
    }

    NSMutableDictionary *properties = [NSMutableDictionary dictionary];
    properties[NSHTTPCookieName] = [NSString stringWithUTF8String:name];
    properties[NSHTTPCookieValue] = [NSString stringWithUTF8String:value];
    properties[NSHTTPCookieDomain] = [NSString stringWithUTF8String:domain];
    properties[NSHTTPCookiePath] = [NSString stringWithUTF8String:path];
    if (secure) {
      properties[NSHTTPCookieSecure] = @"TRUE";
    }
    if (http_only) {
      properties[@"HttpOnly"] = @"TRUE";
    }
    if (has_expires) {
      properties[NSHTTPCookieExpires] =
          [NSDate dateWithTimeIntervalSince1970:expires_unix];
    }

    NSHTTPCookie *cookie = [NSHTTPCookie cookieWithProperties:properties];
    if (cookie == nil) {
      if (err != nullptr) {
        *err = copy_error(@"failed to construct cookie");
      }
      return nullptr;
    }

    __block BOOL done = NO;
    [store setCookie:cookie completionHandler:^{
      done = YES;
    }];
    if (!wait_until_done(&done, 5.0)) {
      if (err != nullptr) {
        *err = copy_error(@"timed out while setting cookie");
      }
    }
    return nullptr;
  }
}

extern "C" char *CgoWebViewDeleteCookie(webview_t w, const char *name,
                                        const char *domain, const char *path) {
  @autoreleasepool {
    if (![NSThread isMainThread]) {
      return copy_error(@"cookie APIs must be called on the UI thread");
    }

    WKHTTPCookieStore *store = cookie_store_for_webview(w);
    if (store == nil) {
      return copy_error(@"failed to resolve WKHTTPCookieStore");
    }

    NSString *target_name = [NSString stringWithUTF8String:name];
    NSString *target_domain =
        [[NSString stringWithUTF8String:domain] lowercaseString];
    NSString *target_path = [NSString stringWithUTF8String:path];

    __block BOOL fetched = NO;
    __block NSArray<NSHTTPCookie *> *cookies = nil;
    [store getAllCookies:^(NSArray<NSHTTPCookie *> *all_cookies) {
      cookies = all_cookies;
      fetched = YES;
    }];
    if (!wait_until_done(&fetched, 5.0)) {
      return copy_error(@"timed out while reading cookies");
    }

    __block NSUInteger pending = 0;
    __block BOOL delete_done = NO;
    for (NSHTTPCookie *cookie in cookies) {
      if (![cookie.name isEqualToString:target_name]) {
        continue;
      }
      if (![[cookie.domain lowercaseString] isEqualToString:target_domain]) {
        continue;
      }
      if (![cookie.path isEqualToString:target_path]) {
        continue;
      }
      pending++;
      [store deleteCookie:cookie completionHandler:^{
        pending--;
        if (pending == 0) {
          delete_done = YES;
        }
      }];
    }
    if (pending == 0) {
      return nullptr;
    }
    if (!wait_until_done(&delete_done, 5.0)) {
      return copy_error(@"timed out while deleting cookie");
    }
    return nullptr;
  }
}

extern "C" char *CgoWebViewClearCookies(webview_t w) {
  @autoreleasepool {
    if (![NSThread isMainThread]) {
      return copy_error(@"cookie APIs must be called on the UI thread");
    }

    WKHTTPCookieStore *store = cookie_store_for_webview(w);
    if (store == nil) {
      return copy_error(@"failed to resolve WKHTTPCookieStore");
    }

    __block BOOL fetched = NO;
    __block NSArray<NSHTTPCookie *> *cookies = nil;
    [store getAllCookies:^(NSArray<NSHTTPCookie *> *all_cookies) {
      cookies = all_cookies;
      fetched = YES;
    }];
    if (!wait_until_done(&fetched, 5.0)) {
      return copy_error(@"timed out while reading cookies");
    }

    __block NSUInteger pending = 0;
    __block BOOL delete_done = NO;
    for (NSHTTPCookie *cookie in cookies) {
      pending++;
      [store deleteCookie:cookie completionHandler:^{
        pending--;
        if (pending == 0) {
          delete_done = YES;
        }
      }];
    }
    if (pending == 0) {
      return nullptr;
    }
    if (!wait_until_done(&delete_done, 5.0)) {
      return copy_error(@"timed out while clearing cookies");
    }
    return nullptr;
  }
}

extern "C" void CgoWebViewFreeString(char *s) { free(s); }
