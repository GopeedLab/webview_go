//go:build darwin

package webview

// IsAvailable always returns true on macOS because WebKit is a built-in
// system framework.
func IsAvailable() bool {
	return true
}
