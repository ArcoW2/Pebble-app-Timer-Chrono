// layout.h - line boxes and digit geometry. Pure C (no pebble.h).
//
// Lines are stacked as one block and centred vertically; gaps depend on
// the adjacent line sizes; the remaining height is split top/bottom.
// Digits are fitted per line from the worst-case format and never change
// while running.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "settings.h"

#define SCREEN_W 200
#define SCREEN_H 228
#define HINT_W   22   // right hint column
#define CORNER   22   // top-left Back hint, bottom-left mode arrow

typedef struct {
  int16_t h;         // digit height
  int16_t w;         // digit cell width
  int16_t t;         // segment thickness
  int16_t narrow_w;  // ':' '.' cell width
  int16_t spacing;   // gap between cells
} DigitGeom;

typedef struct {
  int16_t   y, h;         // line box
  int16_t   gutter_w;     // indicator column
  int16_t   text_x;       // left edge of the digit text (right aligned)
  int16_t   text_w;
  int16_t   text_y;
  DigitGeom dg;
} LineGeom;

typedef enum { LAYOUT_OK = 0, LAYOUT_TOO_TALL, LAYOUT_TOO_WIDE } LayoutError;

typedef struct {
  uint8_t  n;
  uint8_t  error;       // LayoutError
  uint8_t  error_line;  // 0-based line index for LAYOUT_TOO_WIDE
  LineGeom g[MAX_LINES];
} Layout;

// Fits `wide` + `narrow` cells into avail_w at most max_h high.
bool layout_fit_digits(uint8_t wide, uint8_t narrow, int16_t avail_w, int16_t max_h,
                       int16_t min_h, DigitGeom *out);

int16_t layout_text_width(uint8_t wide, uint8_t narrow, const DigitGeom *dg);

// fmts[i] is the resolved (or worst-case) format of line i.
bool layout_compute(const ModeLayout *ml, const uint8_t *fmts, Layout *out);
