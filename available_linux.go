//go:build linux

package webview

/*
#include <dlfcn.h>

static int check_webview_available() {
  // webkit2gtk 4.0 (GTK 3)
  void *lib = dlopen("libwebkit2gtk-4.0.so", RTLD_LAZY);
  if (lib) { dlclose(lib); return 1; }
  // webkit2gtk 4.1 (GTK 4)
  lib = dlopen("libwebkit2gtk-4.1.so", RTLD_LAZY);
  if (lib) { dlclose(lib); return 1; }
  return 0;
}
*/
import "C"

// IsAvailable checks whether the required webkit2gtk runtime library is
// present on this system by attempting to dlopen it.
func IsAvailable() bool {
	return C.check_webview_available() != 0
}
