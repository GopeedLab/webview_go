//go:build webview_integration
// +build webview_integration

package webview

import (
	"fmt"
	"testing"
	"time"
)

func TestNativeUserClosedEvent(t *testing.T) {
	done := make(chan error, 1)
	testMainTasks <- func() {
		w := New(false)
		w.SetSize(320, 200, HintNone)
		defer w.Destroy()
		count := 0
		w.SetEventHandler(func(e Event) {
			if e.Name == "closed" {
				count++
			}
		})
		w.Dispatch(func() { closeEventTestWindow(w) })
		timer := time.AfterFunc(5*time.Second, func() { w.Dispatch(func() { w.Terminate() }) })
		defer timer.Stop()
		w.Run()
		if count != 1 {
			done <- fmt.Errorf("closed event count: %d", count)
			return
		}
		done <- nil
	}
	if err := <-done; err != nil {
		t.Fatal(err)
	}
}
