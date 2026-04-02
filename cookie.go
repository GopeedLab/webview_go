package webview

import "time"

// Cookie describes a browser cookie exposed by the native cookie store.
type Cookie struct {
	Name     string
	Value    string
	Domain   string
	Path     string
	Expires  time.Time
	Secure   bool
	HTTPOnly bool
}
