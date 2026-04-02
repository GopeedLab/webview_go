//go:build darwin || linux || windows

package webview

/*
#cgo darwin CXXFLAGS: -x objective-c++ -fblocks
#cgo darwin LDFLAGS: -framework Foundation

#include "webview.h"
#include <stdlib.h>

char *CgoWebViewGetCookies(webview_t w, const char *url, char **err);
char *CgoWebViewSetCookie(webview_t w, const char *name, const char *value, const char *domain, const char *path, int httpOnly, int secure, int hasExpires, double expiresUnix, char **err);
char *CgoWebViewDeleteCookie(webview_t w, const char *name, const char *domain, const char *path);
char *CgoWebViewClearCookies(webview_t w);
void CgoWebViewFreeString(char *s);
*/
import "C"

import (
	"errors"
	"unsafe"
)

func nativeError(cerr *C.char) error {
	if cerr == nil {
		return nil
	}
	defer C.CgoWebViewFreeString(cerr)
	return errors.New(C.GoString(cerr))
}

func (w *webview) GetCookies(url string) ([]Cookie, error) {
	curl := C.CString(url)
	defer C.free(unsafe.Pointer(curl))

	var cerr *C.char
	cjson := C.CgoWebViewGetCookies(w.w, curl, &cerr)
	if err := nativeError(cerr); err != nil {
		return nil, err
	}
	if cjson == nil {
		return nil, nil
	}
	defer C.CgoWebViewFreeString(cjson)
	return decodeCookiesJSON(C.GoString(cjson))
}

func (w *webview) SetCookie(cookie Cookie) error {
	prepared, err := prepareCookieForNative(cookie)
	if err != nil {
		return err
	}

	cname := C.CString(prepared.Name)
	defer C.free(unsafe.Pointer(cname))
	cvalue := C.CString(prepared.Value)
	defer C.free(unsafe.Pointer(cvalue))
	cdomain := C.CString(prepared.Domain)
	defer C.free(unsafe.Pointer(cdomain))
	cpath := C.CString(prepared.Path)
	defer C.free(unsafe.Pointer(cpath))

	hasExpires := C.int(0)
	expiresUnix := C.double(0)
	if prepared.HasExpires {
		hasExpires = 1
		expiresUnix = C.double(prepared.ExpiresUnix)
	}

	var cerr *C.char
	C.CgoWebViewSetCookie(
		w.w,
		cname,
		cvalue,
		cdomain,
		cpath,
		boolToInt(prepared.HTTPOnly),
		boolToInt(prepared.Secure),
		hasExpires,
		expiresUnix,
		&cerr,
	)
	return nativeError(cerr)
}

func (w *webview) DeleteCookie(name string, domain string, path string) error {
	prepared, err := prepareCookieIdentity(name, domain, path)
	if err != nil {
		return err
	}

	cname := C.CString(prepared.Name)
	defer C.free(unsafe.Pointer(cname))
	cdomain := C.CString(prepared.Domain)
	defer C.free(unsafe.Pointer(cdomain))
	cpath := C.CString(prepared.Path)
	defer C.free(unsafe.Pointer(cpath))

	cerr := C.CgoWebViewDeleteCookie(w.w, cname, cdomain, cpath)
	return nativeError(cerr)
}

func (w *webview) ClearCookies() error {
	cerr := C.CgoWebViewClearCookies(w.w)
	return nativeError(cerr)
}
