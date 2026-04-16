// Stubs for non-Windows platforms.
#ifndef _WIN32

extern "C" void CgoWebViewSetVisible(void *w, int visible) {
  (void)w;
  (void)visible;
}

extern "C" void CgoWebViewEnableWindowClose(void *w) { (void)w; }

extern "C" void *CgoWebViewCreateHiddenWindow() { return nullptr; }

extern "C" void CgoWebViewDestroyHiddenWindow(void *hwnd) { (void)hwnd; }

#endif
