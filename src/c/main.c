// main.c - app lifecycle.
//
// The counter keeps running while the app is closed: it is only
// timestamps. Alarms come back as wakeups.
#include <pebble.h>

#include "alarms.h"
#include "app.h"
#include "layout.h"
#include "comm.h"
#include "win_editor.h"
#include "win_main.h"

// A throwaway window is the portable way to ask how big the screen is.
static Window *s_probe;

static void measure_screen(void) {
  s_probe = window_create();
  GRect b = layer_get_bounds(window_get_root_layer(s_probe));
  layout_set_screen(b.size.w, b.size.h);
  window_destroy(s_probe);
  s_probe = NULL;
}

static void init(void) {
  // Everything lays out from this: 200x228 on emery, 144x168 elsewhere.
  measure_screen();
  app_load();
  comm_open();
  win_main_create_and_push();

  bool from_wakeup = (launch_reason() == APP_LAUNCH_WAKEUP);
  alarms_start(from_wakeup);
  // Opened by hand with nothing set up: go straight to the mode choice.
  if (!from_wakeup && g_counter.state == ST_IDLE) win_editor_push_setup();
}

static void deinit(void) {
  alarms_stop_and_schedule_wakeup();
  app_save_counter();
  win_main_destroy();
  // The other windows destroy themselves in their unload handler.
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
