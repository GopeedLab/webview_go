# webview_go

[![GoDoc](https://godoc.org/github.com/GopeedLab/webview_go?status.svg)](https://godoc.org/github.com/GopeedLab/webview_go)
[![Go Report Card](https://goreportcard.com/badge/github.com/GopeedLab/webview_go)](https://goreportcard.com/report/github.com/GopeedLab/webview_go)

Fork of [webview/webview_go][upstream] with extra features. Go language binding for the [webview library][webview].

### What's New

- **Cookie Management** — Full cross-platform cookie APIs (`GetCookies`, `SetCookie`, `DeleteCookie`, `ClearCookies`) with native implementations for macOS (WebKit), Linux (WebKitGTK), and Windows (WebView2).
- **SetUserAgent** — Update the native user agent string used by the embedded browser engine.

### Getting Started

See [Go package documentation][go-docs] for the Go API documentation, or simply read the source code.

Start with creating a new directory structure for your project.

```sh
mkdir my-project && cd my-project
```

Create a new Go module.

```sh
go mod init example.com/app
```

Save one of the example programs into your project directory.

```sh
curl -sSLo main.go "https://raw.githubusercontent.com/GopeedLab/webview_go/master/examples/basic/main.go"
```

Install dependencies.

```sh
go get github.com/GopeedLab/webview_go
```

Build the example. On Windows, add `-ldflags="-H windowsgui"` to the command line.

```sh
go build
```

### Notes

Calling `Eval()` or `Dispatch()` before `Run()` does not work because the webview instance has only been configured and not yet started.

Cookie APIs are available through `GetCookies`, `SetCookie`, `DeleteCookie`, and `ClearCookies` on `darwin`, `linux`, and `windows`. They should be called on the UI thread.

[go-docs]: https://pkg.go.dev/github.com/GopeedLab/webview_go
[upstream]: https://github.com/webview/webview_go
[webview]: https://github.com/webview/webview

### Host-owned profiles and proxy routing

`NewWithOptions` accepts an absolute profile data directory and an HTTP or SOCKS5
proxy endpoint before the native browser is created. Reuse the directory for
pages that should share cookies and website storage; use different directories
for independent profiles. The native Cookie APIs use the selected browser store.

On macOS, named persistent stores and proxy configuration require macOS 14+.
Use a SOCKS5 endpoint on Apple platforms to route both HTTP and HTTPS. An HTTP
CONNECT configuration may leave plain HTTP outside the configured proxy.
Windows uses a per-environment user data folder and browser arguments; GTK uses
a per-profile website data manager and context. No process-wide proxy/profile
environment variables are modified.

The host may supply a loopback forwarding proxy to keep upstream credentials and
routing policy out of the browser. `ProxyURL` itself does not accept credentials.
Calls retain the same platform UI-thread requirements as `New`.

Run `go test -tags webview_integration -v -count=1 .` on a desktop session (or under `xvfb-run` on
Linux) to verify routing, native cookies, profile isolation, and reopening.

Host applications can call `RemoveProfile(dataPath)` after destroying all WebViews
using that profile. It removes the named WebKit store on macOS, releases and clears
the GTK profile context, and deletes the Windows/Linux data directory. Like browser
creation, profile removal belongs to the host lifecycle, not page JavaScript.
