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
