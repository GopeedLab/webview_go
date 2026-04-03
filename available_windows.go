//go:build windows

package webview

/*
extern int CgoWebViewIsAvailable(void);
*/
import "C"

// IsAvailable checks whether the WebView2 runtime is available on this system.
// It first tries the official GetAvailableCoreWebView2BrowserVersionString API
// via WebView2Loader.dll. If that is not present, it falls back to checking the
// registry for the WebView2 runtime install path and verifying the runtime DLL
// actually exists on disk.
func IsAvailable() bool {
	return C.CgoWebViewIsAvailable() != 0
}
