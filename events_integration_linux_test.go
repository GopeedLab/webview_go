//go:build webview_integration

package webview

import (
	"fmt"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"
)

func TestGTKCallbacksDetachedAfterDestroy(t *testing.T) {
	for _, userClose := range []bool{false, true} {
		name := "api"
		if userClose {
			name = "user"
		}
		t.Run(name, func(t *testing.T) {
			done := make(chan error, 1)
			testMainTasks <- func() { done <- checkGTKCallbackCleanup(userClose) }
			if err := <-done; err != nil {
				t.Fatal(err)
			}
		})
	}
}

func TestGTKHandledFailureDoesNotLoadFallbackDocument(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		_, _ = fmt.Fprint(w, "<!doctype html><title>origin</title>")
	}))
	defer server.Close()
	done := make(chan error, 1)
	testMainTasks <- func() {
		w := NewHeadless(false)
		defer w.Destroy()
		failed, fallback := false, false
		var settle *time.Timer
		w.SetEventHandler(func(event Event) {
			if event.Name == "load-error" && !failed {
				failed = true
				// Leave the loop running so a default error document would load.
				settle = time.AfterFunc(300*time.Millisecond, func() { w.Dispatch(w.Terminate) })
			}
		})
		if err := w.Bind("reportDocument", func(url string) {
			if failed {
				fallback = true
			} else if url == server.URL+"/" {
				w.Navigate(strings.Replace(server.URL, "http://", "https://", 1) + "/failure")
			}
		}); err != nil {
			done <- err
			return
		}
		w.Init(`window.addEventListener("load", () => reportDocument(location.href));`)
		timer := time.AfterFunc(10*time.Second, func() { w.Dispatch(w.Terminate) })
		w.Navigate(server.URL + "/")
		w.Run()
		timer.Stop()
		if settle != nil {
			settle.Stop()
		}
		if !failed || fallback {
			done <- fmt.Errorf("failed=%v, fallback document loaded=%v", failed, fallback)
			return
		}
		done <- nil
	}
	if err := <-done; err != nil {
		t.Fatal(err)
	}
}
