// Stubs for non-Windows platforms.
#ifndef _WIN32

#ifdef WEBVIEW_GTK
#include <gtk/gtk.h>
#endif

extern "C" void CgoWebViewSetVisible(void *w, int visible) {
  (void)w;
  (void)visible;
}

extern "C" void CgoWebViewEnableWindowClose(void *w) { (void)w; }

extern "C" void *CgoWebViewCreateHiddenWindow() {
#ifdef WEBVIEW_GTK
  if (gtk_init_check(nullptr, nullptr) == FALSE) {
    return nullptr;
  }
  auto *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size(GTK_WINDOW(window), 640, 480);
  gtk_widget_realize(window);
  return window;
#else
  return nullptr;
#endif
}

extern "C" void CgoWebViewDestroyHiddenWindow(void *hwnd) {
#ifdef WEBVIEW_GTK
  if (hwnd) {
    gtk_widget_destroy(static_cast<GtkWidget *>(hwnd));
  }
#else
  (void)hwnd;
#endif
}

#endif
