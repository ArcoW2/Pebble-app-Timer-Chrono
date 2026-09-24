// icons.h - icons drawn with primitives, so they scale with the box size.
#pragma once

#include <pebble.h>

typedef enum {
  IC_NONE = 0,
  // line indicators
  IC_TOTAL, IC_REMAIN, IC_LAP, IC_PRIOR, IC_PRIOR2, IC_BEST, IC_AVG, IC_DELTA,
  IC_UNTIL, IC_COUNT,
  // actions
  IC_PLAY, IC_PAUSE, IC_STOP, IC_RESET, IC_RESTART, IC_EDIT, IC_SETTINGS,
  IC_PRESETS, IC_NEXT, IC_PLUS, IC_MINUS, IC_CHECK, IC_UP, IC_DOWN, IC_EXIT,
  IC_BACK, IC_CANCEL,
  // mode indicator
  IC_MODE_UP, IC_MODE_DOWN, IC_MODE_UNTIL,
  IC_COUNT_ALL
} IconId;

void icon_draw(GContext *ctx, uint8_t id, GRect box, GColor color);

// Indicator icon and short label for a line source.
uint8_t     icon_for_source(uint8_t source);
const char *label_for_source(uint8_t source);
