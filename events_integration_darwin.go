//go:build webview_integration
// +build webview_integration

package webview

/*
#include <objc/message.h>
#include <objc/runtime.h>
static void event_test_close_window(void *window) {
 ((void (*)(void *, SEL, void *))objc_msgSend)(window, sel_registerName("performClose:"), 0);
}
*/
import "C"

func closeEventTestWindow(w WebView) { C.event_test_close_window(w.Window()) }
