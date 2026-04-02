//go:build !darwin && !linux && !windows

package webview

import "errors"

var errCookieManagerUnsupported = errors.New("cookie APIs are only implemented on darwin, linux, and windows")

func (w *webview) GetCookies(url string) ([]Cookie, error) {
	return nil, errCookieManagerUnsupported
}

func (w *webview) SetCookie(cookie Cookie) error {
	return errCookieManagerUnsupported
}

func (w *webview) DeleteCookie(name string, domain string, path string) error {
	return errCookieManagerUnsupported
}

func (w *webview) ClearCookies() error {
	return errCookieManagerUnsupported
}
