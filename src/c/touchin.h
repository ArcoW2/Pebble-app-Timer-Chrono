// touchin.h - touch recognizer on top of touch_service: taps, horizontal
// swipes and circular swipes (clockwise = up) with speed-based steps.
//
// Tap rules (PLAN.md, Touch rules): short and nearly still, not a
// non-navigational contact (idle wrist contact), and not within the grace
// period after the window appeared. The client decides whether the target
// under the tap is valid.
#pragma once

#include <pebble.h>

#define SWIPE_LEFT  (-1)
#define SWIPE_RIGHT (+1)

typedef struct {
  void (*on_tap)(GPoint where, void *ctx);
  void (*on_swipe)(int8_t direction, void *ctx);
  void (*on_rotate)(int16_t steps, void *ctx);  // NULL: no circular swipes
  void *ctx;
} TouchClient;

// One client at a time: attach on window appear, detach on disappear.
// Detach only acts if `ctx` is still the attached client, so the order of
// appear/disappear between two windows does not matter.
void touchin_attach(const TouchClient *client);
void touchin_detach(void *ctx);
