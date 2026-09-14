package webview

import (
	"flag"
	"log"
	"os"
	"testing"
)

func Example() {
	w := New(true)
	defer w.Destroy()
	w.SetTitle("Hello")
	w.Bind("noop", func() string {
		log.Println("hello")
		return "hello"
	})
	w.Bind("add", func(a, b int) int {
		return a + b
	})
	w.Bind("quit", func() {
		w.Terminate()
	})
	w.SetHtml(`<!doctype html>
		<html>
			<body>hello</body>
			<script>
				window.onload = function() {
					document.body.innerText = ` + "`hello, ${navigator.userAgent}`" + `;
					noop().then(function(res) {
						console.log('noop res', res);
						add(1, 2).then(function(res) {
							console.log('add res', res);
							quit();
						});
					});
				};
			</script>
		</html>
	)`)
	w.Run()
}

var testMainTasks = make(chan func())

func TestMain(m *testing.M) {
	flag.Parse()
	if testing.Verbose() {
		Example()
	}
	// Native integration tests must create and destroy views on the main OS
	// thread. Test functions run on testing goroutines and dispatch work here.
	finished := make(chan int, 1)
	go func() { finished <- m.Run() }()
	for {
		select {
		case task := <-testMainTasks:
			task()
		case code := <-finished:
			os.Exit(code)
		}
	}
}
