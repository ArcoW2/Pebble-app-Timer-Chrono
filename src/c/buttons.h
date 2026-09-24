// buttons.h - button engine and on-screen hints.
//
// Each window gives a ButtonMap: per button a short and a long action with
// icons. The hint column shows both icons (same size), the long icon with
// an arrow under it that fills as hold progress. Rules:
// - a button without a long action fires on press (precise lap times)
// - a button with a long action decides on release (short / long)
// - repeat buttons (+/-) fire on press and repeat with acceleration
// - the press time is passed along, so a lap uses the moment of pressing
#pragma once

#include <pebble.h>

#define BTN_UP     0
#define BTN_SELECT 1
#define BTN_DOWN   2
#define BTN_COUNT  3

#define LONG_PRESS_MS 600

typedef struct {
  uint8_t short_icon;
  uint8_t long_icon;
  uint8_t short_act;   // 0 = none
  uint8_t long_act;    // 0 = none
  bool    repeat;      // short action repeats while held
} ButtonDef;

typedef struct {
  ButtonDef b[BTN_COUNT];
  uint8_t   back_icon;  // top-left hint; IC_NONE hides it
  uint8_t   back_act;   // 0 = pop the window
} ButtonMap;

typedef void (*ButtonActionFn)(uint8_t action, int64_t press_ms, void *ctx);

typedef struct {
  Window        *window;
  Layer         *hints;
  ButtonMap      map;
  ButtonActionFn fn;
  void          *ctx;
  GColor         fg;
  GColor         accent;
  int8_t         held;       // button index, -1 = none
  int64_t        press_ms;
  AppTimer      *timer;
  uint16_t       repeats;
} Buttons;

void buttons_init(Buttons *b, Window *window, ButtonActionFn fn, void *ctx);
void buttons_set_map(Buttons *b, const ButtonMap *map);
void buttons_set_colors(Buttons *b, GColor fg, GColor accent);
void buttons_deinit(Buttons *b);
