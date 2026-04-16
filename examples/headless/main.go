package main

import (
	"fmt"

	webview "github.com/GopeedLab/webview_go"
)

func main() {
	if !webview.IsAvailable() {
		panic("webview is not available on this system")
	}

	w := webview.NewHeadless(false)
	defer w.Destroy()
	w.SetSize(800, 600, webview.HintNone)

	// Bind a Go function so JS can pass results back.
	w.Bind("reportResult", func(result string) {
		fmt.Println("JS reported:", result)
		w.Terminate() // exit the Run() loop
	})

	w.Navigate("data:text/html,<script>document.title='Hello from Headless';window.reportResult(document.title);</script>")

	// Run() must be called on the main goroutine (UI thread).
	// It blocks until Terminate() is called from the binding callback above.
	w.Run()

	fmt.Println("headless session complete")
}
