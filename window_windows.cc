#define WEBVIEW_HEADER
#include "webview.h"

#include <windows.h>

#include "WebView2.h"
#include "EventToken.h"

extern "C" void CgoWebViewSetVisible(webview_t w, int visible) {
  auto hwnd = static_cast<HWND>(
      webview_get_native_handle(w, WEBVIEW_NATIVE_HANDLE_KIND_UI_WINDOW));
  if (hwnd) {
    ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
  }
}

// ---- Hidden window for headless mode ----

static LRESULT CALLBACK HeadlessWndProc(HWND hwnd, UINT msg, WPARAM wp,
                                        LPARAM lp) {
  if (msg == WM_CLOSE) {
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

extern "C" void *CgoWebViewCreateHiddenWindow() {
  // Initialise COM so that WebView2 can function even though the webview
  // constructor skips CoInitializeEx when a non-null window is supplied.
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

  HINSTANCE hInstance = GetModuleHandleW(nullptr);
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.hInstance = hInstance;
    wc.lpszClassName = L"webview_headless";
    wc.lpfnWndProc = HeadlessWndProc;
    RegisterClassExW(&wc);
    registered = true;
  }
  return CreateWindowExW(0, L"webview_headless", L"", WS_OVERLAPPEDWINDOW, 0,
                         0, 0, 0, nullptr, nullptr, hInstance, nullptr);
}

extern "C" void CgoWebViewDestroyHiddenWindow(void *hwnd) {
  if (hwnd) {
    DestroyWindow(static_cast<HWND>(hwnd));
  }
  CoUninitialize();
}

// ---- window.close() support ----

namespace {

class window_close_handler
    : public ICoreWebView2WindowCloseRequestedEventHandler {
public:
  explicit window_close_handler(HWND hwnd) : m_hwnd(hwnd), m_ref_count(1) {}

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

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) override {
    if (ppvObject == nullptr) {
      return E_POINTER;
    }
    *ppvObject = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ICoreWebView2WindowCloseRequestedEventHandler)) {
      *ppvObject = static_cast<ICoreWebView2WindowCloseRequestedEventHandler *>(
          this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 * /*sender*/,
                                   IUnknown * /*args*/) override {
    PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
    return S_OK;
  }

private:
  HWND m_hwnd;
  LONG m_ref_count;
};

} // namespace

extern "C" void CgoWebViewEnableWindowClose(webview_t w) {
  auto *controller = static_cast<ICoreWebView2Controller *>(
      webview_get_native_handle(w,
                                WEBVIEW_NATIVE_HANDLE_KIND_BROWSER_CONTROLLER));
  if (controller == nullptr) {
    return;
  }

  ICoreWebView2 *webview = nullptr;
  HRESULT hr = controller->get_CoreWebView2(&webview);
  if (FAILED(hr) || webview == nullptr) {
    return;
  }

  HWND hwnd = static_cast<HWND>(
      webview_get_native_handle(w, WEBVIEW_NATIVE_HANDLE_KIND_UI_WINDOW));

  auto *handler = new window_close_handler(hwnd);
  EventRegistrationToken token{};
  webview->add_WindowCloseRequested(handler, &token);
  handler->Release();
}
