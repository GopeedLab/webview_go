#define WEBVIEW_HEADER
#include "webview.h"
#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>
#import <Network/Network.h>
#include <stdexcept>

static NSMutableDictionary *profile_stores() {
  static NSMutableDictionary *stores = [[NSMutableDictionary alloc] init]; return stores;
}
static NSMutableDictionary *profile_proxies() {
  static NSMutableDictionary *proxies = [[NSMutableDictionary alloc] init]; return proxies;
}

extern "C" void CgoWebViewConfigureDataStore(void *raw, const webview_options_t *options) {
  if (!options) return;
  if (@available(macOS 14.0, *)) {
    WKWebViewConfiguration *config = (WKWebViewConfiguration *)raw;
    NSString *identifier = [NSString stringWithUTF8String:options->profile_id];
    NSUUID *uuid = [[[NSUUID alloc] initWithUUIDString:identifier] autorelease];
    if (!uuid) throw std::runtime_error("invalid profile identifier");
    NSMutableDictionary *stores = profile_stores();
    NSMutableDictionary *proxyURLs = profile_proxies();
    WKWebsiteDataStore *store = [stores objectForKey:identifier];
    if (!store) {
      store = [WKWebsiteDataStore dataStoreForIdentifier:uuid];
      [stores setObject:store forKey:identifier];
    }
    config.websiteDataStore = store;
    NSString *proxyURL = [NSString stringWithUTF8String:options->proxy_url ? options->proxy_url : ""];
    if ([[proxyURLs objectForKey:identifier] isEqualToString:proxyURL]) return;
    if (options->proxy_url && *options->proxy_url) {
      NSURL *url = [NSURL URLWithString:[NSString stringWithUTF8String:options->proxy_url]];
      nw_endpoint_t endpoint = nw_endpoint_create_host(url.host.UTF8String, url.port.stringValue.UTF8String);
      nw_proxy_config_t proxy = [url.scheme isEqualToString:@"socks5"]
          ? nw_proxy_config_create_socksv5(endpoint)
          : nw_proxy_config_create_http_connect(endpoint, nullptr);
      config.websiteDataStore.proxyConfigurations = @[proxy];
      nw_release(proxy);
      nw_release(endpoint);
    } else {
      store.proxyConfigurations = @[];
    }
    [proxyURLs setObject:proxyURL forKey:identifier];
  } else {
    throw std::runtime_error("isolated profiles and proxy require macOS 14 or newer");
  }
}

extern "C" int CgoWebViewRemoveDataStore(const char *rawIdentifier) {
  if (@available(macOS 14.0, *)) {
    @autoreleasepool {
      NSString *identifier = [NSString stringWithUTF8String:rawIdentifier];
      NSUUID *uuid = [[[NSUUID alloc] initWithUUIDString:identifier] autorelease];
      if (!uuid) return -1;
      [profile_stores() removeObjectForKey:identifier];
      [profile_proxies() removeObjectForKey:identifier];
      // WebKit releases its network process reference asynchronously after
      // the last view/store is released. Retry while the run loop drains.
      NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:10];
      do {
        __block BOOL done = NO;
        __block BOOL success = NO;
        [WKWebsiteDataStore removeDataStoreForIdentifier:uuid completionHandler:^(NSError *error) {
          success = error == nil;
          done = YES;
        }];
        while (!done && [deadline timeIntervalSinceNow] > 0) {
          @autoreleasepool {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
          }
        }
        if (done && success) return 0;
        NSDate *retry = [NSDate dateWithTimeIntervalSinceNow:0.1];
        while ([retry timeIntervalSinceNow] > 0) {
          @autoreleasepool {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:retry];
          }
        }
      } while ([deadline timeIntervalSinceNow] > 0);
      return -1;
    }
  }
  // These OS versions cannot have created a named persistent profile.
  return 0;
}
