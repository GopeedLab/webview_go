package webview

import (
	"reflect"
	"testing"
	"time"
)

func TestDefaultCookiePath(t *testing.T) {
	if got := defaultCookiePath(""); got != "/" {
		t.Fatalf("defaultCookiePath(\"\") = %q, want /", got)
	}
	if got := defaultCookiePath("/app"); got != "/app" {
		t.Fatalf("defaultCookiePath(\"/app\") = %q, want /app", got)
	}
}

func TestPrepareCookieForNative(t *testing.T) {
	expires := time.Unix(1710000000, 0)
	prepared, err := prepareCookieForNative(Cookie{
		Name:     "sid",
		Value:    "abc",
		Domain:   ".example.com",
		Secure:   true,
		HTTPOnly: true,
		Expires:  expires,
	})
	if err != nil {
		t.Fatalf("prepareCookieForNative returned error: %v", err)
	}
	if prepared.Path != "/" {
		t.Fatalf("prepared.Path = %q, want /", prepared.Path)
	}
	if !prepared.HasExpires {
		t.Fatal("prepared.HasExpires = false, want true")
	}
	if prepared.ExpiresUnix != float64(expires.Unix()) {
		t.Fatalf("prepared.ExpiresUnix = %v, want %v", prepared.ExpiresUnix, float64(expires.Unix()))
	}
}

func TestPrepareCookieForNativeValidation(t *testing.T) {
	if _, err := prepareCookieForNative(Cookie{Domain: ".example.com"}); err == nil {
		t.Fatal("expected missing-name error")
	}
	if _, err := prepareCookieForNative(Cookie{Name: "sid"}); err == nil {
		t.Fatal("expected missing-domain error")
	}
}

func TestPrepareCookieIdentity(t *testing.T) {
	identity, err := prepareCookieIdentity("sid", ".example.com", "")
	if err != nil {
		t.Fatalf("prepareCookieIdentity returned error: %v", err)
	}
	want := cookieIdentity{Name: "sid", Domain: ".example.com", Path: "/"}
	if !reflect.DeepEqual(identity, want) {
		t.Fatalf("prepareCookieIdentity = %+v, want %+v", identity, want)
	}
}

func TestPrepareCookieIdentityValidation(t *testing.T) {
	if _, err := prepareCookieIdentity("", ".example.com", "/"); err == nil {
		t.Fatal("expected missing-name error")
	}
	if _, err := prepareCookieIdentity("sid", "", "/"); err == nil {
		t.Fatal("expected missing-domain error")
	}
}

func TestDecodeCookiesJSON(t *testing.T) {
	raw := `[{"name":"sid","value":"abc","domain":".example.com","path":"/","expires_unix":1710000000,"secure":true,"http_only":true},{"name":"theme","value":"dark","domain":"example.com","path":"/prefs","secure":false,"http_only":false}]`
	got, err := decodeCookiesJSON(raw)
	if err != nil {
		t.Fatalf("decodeCookiesJSON returned error: %v", err)
	}
	if len(got) != 2 {
		t.Fatalf("len(got) = %d, want 2", len(got))
	}
	if got[0].Name != "sid" || !got[0].Secure || !got[0].HTTPOnly {
		t.Fatalf("unexpected first cookie: %+v", got[0])
	}
	if got[0].Expires.Unix() != 1710000000 {
		t.Fatalf("got[0].Expires = %v, want unix 1710000000", got[0].Expires.Unix())
	}
	if !got[1].Expires.IsZero() {
		t.Fatalf("got[1].Expires = %v, want zero time", got[1].Expires)
	}
}
