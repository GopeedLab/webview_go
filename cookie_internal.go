package webview

import (
	"encoding/json"
	"errors"
	"time"
)

type nativeCookie struct {
	Name        string `json:"name"`
	Value       string `json:"value"`
	Domain      string `json:"domain"`
	Path        string `json:"path"`
	ExpiresUnix *int64 `json:"expires_unix"`
	Secure      bool   `json:"secure"`
	HTTPOnly    bool   `json:"http_only"`
}

type preparedCookie struct {
	Name        string
	Value       string
	Domain      string
	Path        string
	ExpiresUnix float64
	HasExpires  bool
	Secure      bool
	HTTPOnly    bool
}

type cookieIdentity struct {
	Name   string
	Domain string
	Path   string
}

func defaultCookiePath(path string) string {
	if path == "" {
		return "/"
	}
	return path
}

func decodeCookiesJSON(raw string) ([]Cookie, error) {
	items := []nativeCookie{}
	if err := json.Unmarshal([]byte(raw), &items); err != nil {
		return nil, err
	}

	cookies := make([]Cookie, 0, len(items))
	for _, item := range items {
		cookie := Cookie{
			Name:     item.Name,
			Value:    item.Value,
			Domain:   item.Domain,
			Path:     item.Path,
			Secure:   item.Secure,
			HTTPOnly: item.HTTPOnly,
		}
		if item.ExpiresUnix != nil {
			cookie.Expires = time.Unix(*item.ExpiresUnix, 0)
		}
		cookies = append(cookies, cookie)
	}
	return cookies, nil
}

func prepareCookieForNative(cookie Cookie) (preparedCookie, error) {
	if cookie.Name == "" {
		return preparedCookie{}, errors.New("cookie name is required")
	}
	if cookie.Domain == "" {
		return preparedCookie{}, errors.New("cookie domain is required")
	}

	prepared := preparedCookie{
		Name:     cookie.Name,
		Value:    cookie.Value,
		Domain:   cookie.Domain,
		Path:     defaultCookiePath(cookie.Path),
		Secure:   cookie.Secure,
		HTTPOnly: cookie.HTTPOnly,
	}
	if !cookie.Expires.IsZero() {
		prepared.HasExpires = true
		prepared.ExpiresUnix = float64(cookie.Expires.Unix())
	}
	return prepared, nil
}

func prepareCookieIdentity(name string, domain string, path string) (cookieIdentity, error) {
	if name == "" {
		return cookieIdentity{}, errors.New("cookie name is required")
	}
	if domain == "" {
		return cookieIdentity{}, errors.New("cookie domain is required")
	}
	return cookieIdentity{
		Name:   name,
		Domain: domain,
		Path:   defaultCookiePath(path),
	}, nil
}
