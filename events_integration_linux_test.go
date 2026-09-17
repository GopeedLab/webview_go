//go:build webview_integration

package webview

import "testing"

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
