//go:build webview_integration

package webview

/*
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include "libs/webview/include/webview.h"

typedef struct {
  void *engine;
  GObject *widget;
  GObject *manager;
} event_test_objects;

static event_test_objects event_test_retain_objects(webview_t engine) {
  GObject *widget = G_OBJECT(webview_get_native_handle(engine, WEBVIEW_NATIVE_HANDLE_KIND_UI_WIDGET));
  GObject *manager = G_OBJECT(webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(widget)));
  event_test_objects objects = {engine, g_object_ref(widget), g_object_ref(manager)};
  return objects;
}
static int event_test_has_callbacks(event_test_objects objects) {
  return !!g_signal_handler_find(objects.widget, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, objects.engine) +
         !!g_signal_handler_find(objects.manager, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, objects.engine);
}
static void event_test_release_objects(event_test_objects objects) {
  g_object_unref(objects.manager);
  g_object_unref(objects.widget);
}
static void event_test_close_window(void *window) {
  gtk_widget_destroy(GTK_WIDGET(window));
}
*/
import "C"

import "fmt"

func checkGTKCallbackCleanup(userClose bool) error {
	w := New(false)
	objects := C.event_test_retain_objects(w.(*webview).w)
	defer C.event_test_release_objects(objects)
	if got := int(C.event_test_has_callbacks(objects)); got != 2 {
		w.Destroy()
		return fmt.Errorf("expected widget and manager callbacks before close, got %d", got)
	}
	if userClose {
		C.event_test_close_window(w.Window())
	}
	w.Destroy()
	if got := int(C.event_test_has_callbacks(objects)); got != 0 {
		return fmt.Errorf("destroyed engine still owns %d GTK callback sources", got)
	}
	return nil
}
