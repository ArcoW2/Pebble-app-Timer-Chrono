// win_editor.h - setup, editing a running count down, and a plain
// duration editor for settings values.
#pragma once

#include <pebble.h>
#include <stdbool.h>
#include <stdint.h>

typedef void (*EditorDone)(bool ok, int64_t value_ms, void *ctx);

void win_editor_push_setup(void);     // mode step, then the value
void win_editor_push_running(void);   // full value of a running count down
void win_editor_push_value(const char *title, int64_t value_ms, bool with_hours,
                           EditorDone done, void *ctx);
