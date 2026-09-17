//go:build webview_integration

package webview

import (
	"fmt"
	"net"
	"strings"
	"testing"
	"time"
)

func TestNativeNavigationErrorEvent(t *testing.T) {
	done := make(chan error, 1)
	testMainTasks <- func() {
		listener, err := net.Listen("tcp", "127.0.0.1:0")
		if err != nil {
			done <- err
			return
		}
		target := "http://" + listener.Addr().String() + "/unreachable"
		listener.Close()
		w := NewHeadless(false)
		defer w.Destroy()
		var received Event
		w.SetEventHandler(func(e Event) {
			if e.Name == "load-error" {
				received = e
				w.Terminate()
			}
		})
		timer := time.AfterFunc(10*time.Second, func() { w.Dispatch(func() { w.Terminate() }) })
		defer timer.Stop()
		w.Navigate(target)
		w.Run()
		if received.Name != "load-error" || !strings.Contains(received.URL, "/unreachable") || received.Message == "" {
			done <- fmt.Errorf("unexpected error event: %+v", received)
			return
		}
		// Clearing the callback must be safe before destruction.
		w.SetEventHandler(nil)
		done <- nil
	}
	if err := <-done; err != nil {
		t.Fatal(err)
	}
}
