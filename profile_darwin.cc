#define WEBVIEW_HEADER
#include "webview.h"
#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>
#import <Network/Network.h>
#include <stdexcept>

extern "C" void CgoWebViewConfigureDataStore(void *raw, const webview_options_t *options) {
  if (!options) return;
  if (@available(macOS 14.0, *)) {
    WKWebViewConfiguration *config = (WKWebViewConfiguration *)raw;
    NSString *identifier = [NSString stringWithUTF8String:options->profile_id];
    NSUUID *uuid = [[[NSUUID alloc] initWithUUIDString:identifier] autorelease];
    if (!uuid) throw std::runtime_error("invalid profile identifier");
    config.websiteDataStore = [WKWebsiteDataStore dataStoreForIdentifier:uuid];
    if (options->proxy_url && *options->proxy_url) {
      NSURL *url = [NSURL URLWithString:[NSString stringWithUTF8String:options->proxy_url]];
      nw_endpoint_t endpoint = nw_endpoint_create_host(url.host.UTF8String, url.port.stringValue.UTF8String);
      nw_proxy_config_t proxy = [url.scheme isEqualToString:@"socks5"]
          ? nw_proxy_config_create_socksv5(endpoint)
          : nw_proxy_config_create_http_connect(endpoint, nullptr);
      config.websiteDataStore.proxyConfigurations = @[proxy];
      nw_release(proxy);
      nw_release(endpoint);
    }
  } else {
    throw std::runtime_error("isolated profiles and proxy require macOS 14 or newer");
  }
}
