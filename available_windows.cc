#include <windows.h>

extern "C" int CgoWebViewIsAvailable(void) {
  // 1. Try the official WebView2 SDK loader API
  HMODULE mod = LoadLibraryW(L"WebView2Loader.dll");
  if (mod != nullptr) {
    typedef HRESULT(STDAPICALLTYPE *GetVersionFn)(PCWSTR, LPWSTR *);
    auto fn = reinterpret_cast<GetVersionFn>(
        GetProcAddress(mod,
                       "GetAvailableCoreWebView2BrowserVersionString"));
    if (fn != nullptr) {
      LPWSTR version = nullptr;
      HRESULT hr = fn(nullptr, &version);
      FreeLibrary(mod);
      if (SUCCEEDED(hr) && version != nullptr) {
        CoTaskMemFree(version);
        return 1;
      }
      return 0;
    }
    FreeLibrary(mod);
  }

  // 2. Fallback: check registry for runtime path, verify DLL exists on disk
  static const wchar_t kSubKey[] =
      L"SOFTWARE\\Microsoft\\EdgeUpdate\\ClientState\\"
      L"{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}";

  struct {
    HKEY root;
    REGSAM sam;
  } sources[] = {
      {HKEY_LOCAL_MACHINE, KEY_READ | KEY_WOW64_32KEY},
      {HKEY_LOCAL_MACHINE, KEY_READ | KEY_WOW64_64KEY},
      {HKEY_CURRENT_USER, KEY_READ},
  };

  for (const auto &src : sources) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(src.root, kSubKey, 0, src.sam, &key) != ERROR_SUCCESS) {
      continue;
    }
    wchar_t path[MAX_PATH] = {};
    DWORD size = sizeof(path);
    if (RegQueryValueExW(key, L"EBWebView", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(path), &size) !=
            ERROR_SUCCESS ||
        path[0] == L'\0') {
      RegCloseKey(key);
      continue;
    }
    RegCloseKey(key);

    // Build DLL path (same logic as webview library's make_client_dll_path)
    wchar_t dll_path[MAX_PATH] = {};
    size_t len = wcslen(path);
    if (len == 0 || len >= MAX_PATH) {
      continue;
    }
    wcscpy_s(dll_path, path);
    if (path[len - 1] != L'\\' && path[len - 1] != L'/') {
      wcscat_s(dll_path, L"\\");
    }
    wcscat_s(dll_path, L"EBWebView\\");
#if defined(_M_X64) || defined(__x86_64__)
    wcscat_s(dll_path, L"x64");
#elif defined(_M_IX86) || defined(__i386__)
    wcscat_s(dll_path, L"x86");
#elif defined(_M_ARM64) || defined(__aarch64__)
    wcscat_s(dll_path, L"arm64");
#endif
    wcscat_s(dll_path, L"\\EmbeddedBrowserWebView.dll");

    if (GetFileAttributesW(dll_path) != INVALID_FILE_ATTRIBUTES) {
      return 1;
    }
  }

  return 0;
}
