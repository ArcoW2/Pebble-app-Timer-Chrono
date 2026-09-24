// segment.h - LCD-style seven-segment renderer for LineText.
#pragma once

#include <pebble.h>

#include "format.h"
#include "layout.h"

typedef struct {
  GColor lit;       // colour for lit segments (per line colour class)
  GColor dim;       // CF_DIM cells
  GColor  ghost;        // unlit segments
  uint8_t ghost_level;  // 0 none .. 3 full thickness
} SegColors;

void segment_draw_text(GContext *ctx, const LineText *text, GPoint origin,
                       const DigitGeom *dg, const SegColors *colors);
