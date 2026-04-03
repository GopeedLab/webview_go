package main

import webview "github.com/GopeedLab/webview_go"

func main() {
	if !webview.IsAvailable() {
		panic("webview is not available on this system")
	}
	w := webview.New(false)
	defer w.Destroy()
	w.SetTitle("Basic Example")
	w.SetSize(480, 320, webview.HintNone)
	w.SetHtml("Thanks for using webview!")
	w.Run()
}
