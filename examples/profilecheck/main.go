// profilecheck exercises native profile isolation and proxy routing on a real
// desktop WebView. Run with `go run ./examples/profilecheck` on a desktop session.
package main

import (
	"fmt"
	"io"
	"net"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"sync/atomic"
	"time"

	webview "github.com/GopeedLab/webview_go"
)

func main() {
	fmt.Printf("Native browser available: %v\n", webview.IsAvailable())
	root, err := os.MkdirTemp("", "webview-profiles-")
	if err != nil {
		panic(err)
	}
	defer os.RemoveAll(root)
	var hits, requests int32
	origin := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		atomic.AddInt32(&requests, 1)
		w.Header().Set("Cache-Control", "no-store")
		w.Header().Set("Content-Type", "text/html")
		fmt.Fprint(w, `<script>window.onload=async()=>{
     const before=localStorage.getItem('owner')||'';
     if(!before)localStorage.setItem('owner','saved');
     const cookieBefore=document.cookie.split('; ').filter(Boolean).sort().join('; ');
     document.cookie='native_cookie=yes; max-age=3600; path=/';
     document.cookie='session_cookie=yes; path=/';
     await report(before + '|' + cookieBefore);
   };</script>`)
	}))
	defer origin.Close()
	proxy := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		atomic.AddInt32(&hits, 1)
		if r.Method == "CONNECT" {
			remote, err := net.Dial("tcp", origin.Listener.Addr().String())
			if err != nil {
				http.Error(w, err.Error(), 502)
				return
			}
			defer remote.Close()
			client, buf, err := w.(http.Hijacker).Hijack()
			if err != nil {
				return
			}
			defer client.Close()
			buf.WriteString("HTTP/1.1 200 Connection Established\r\n\r\n")
			buf.Flush()
			go func() { io.Copy(remote, buf); remote.Close() }()
			io.Copy(client, remote)
		} else {
			origin.Config.Handler.ServeHTTP(w, r)
		}
	}))
	defer proxy.Close()
	proxyURL := proxy.URL
	if runtime.GOOS == "darwin" {
		listener, err := net.Listen("tcp", "127.0.0.1:0")
		if err != nil {
			panic(err)
		}
		defer listener.Close()
		proxyURL = "socks5://" + listener.Addr().String()
		go func() {
			for {
				c, err := listener.Accept()
				if err != nil {
					return
				}
				go serveSOCKS(c, origin.Listener.Addr().String(), &hits)
			}
		}()
	}
	for _, tc := range []struct{ profile, want string }{{"a", ""}, {"b", ""}, {"a", "saved"}, {"remove-a", ""}, {"a", ""}, {"b", "saved"}} {
		if tc.profile == "remove-a" {
			if err := webview.RemoveProfile(filepath.Join(root, "a")); err != nil {
				panic(err)
			}
			if err := webview.RemoveProfile(filepath.Join(root, "a")); err != nil {
				panic(err)
			}
			continue
		}
		w, err := webview.NewWithOptions(webview.Options{Headless: true, DataPath: filepath.Join(root, tc.profile), ProxyURL: proxyURL})
		if err != nil {
			panic(err)
		}
		result := make(chan string, 1)
		w.Bind("report", func(value string) { result <- value })
		done := make(chan error, 1)
		go func() {
			select {
			case value := <-result:
				want := tc.want + "|"
				if tc.want != "" {
					want += "native_cookie=yes"
					value = strings.TrimSuffix(value, "; session_cookie=yes")
				}
				if value != want {
					done <- fmt.Errorf("profile %s: got %q want %q", tc.profile, value, want)
					w.Terminate()
					return
				}
				w.Dispatch(func() {
					cookies, err := w.GetCookies("http://profile-test.invalid/")
					if err == nil && len(cookies) == 0 {
						err = fmt.Errorf("native cookie store is empty")
					}
					done <- err
					w.Terminate()
				})
			case <-time.After(20 * time.Second):
				done <- fmt.Errorf("profile navigation timeout")
				w.Terminate()
			}
		}()
		w.Navigate("http://profile-test.invalid/")
		w.Run()
		err = <-done
		w.Destroy()
		if err != nil {
			panic(err)
		}
	}
	if atomic.LoadInt32(&hits) == 0 || atomic.LoadInt32(&requests) < 3 {
		panic("navigation bypassed configured proxy")
	}
	fmt.Println("PASS: proxy routing, native cookies, profile isolation, reopening and deletion")
}

func serveSOCKS(c net.Conn, origin string, hits *int32) {
	defer c.Close()
	c.SetDeadline(time.Now().Add(30 * time.Second))
	var hello [2]byte
	if _, err := io.ReadFull(c, hello[:]); err != nil {
		return
	}
	if _, err := io.CopyN(io.Discard, c, int64(hello[1])); err != nil {
		return
	}
	c.Write([]byte{5, 0})
	var header [4]byte
	if _, err := io.ReadFull(c, header[:]); err != nil {
		return
	}
	count := 4
	switch header[3] {
	case 3:
		var size [1]byte
		if _, err := io.ReadFull(c, size[:]); err != nil {
			return
		}
		count = int(size[0])
	case 4:
		count = 16
	}
	if _, err := io.CopyN(io.Discard, c, int64(count+2)); err != nil {
		return
	}
	remote, err := net.Dial("tcp", origin)
	if err != nil {
		return
	}
	defer remote.Close()
	atomic.AddInt32(hits, 1)
	c.Write([]byte{5, 0, 0, 1, 127, 0, 0, 1, 0, 0})
	go func() { io.Copy(remote, c); remote.Close() }()
	io.Copy(c, remote)
}
