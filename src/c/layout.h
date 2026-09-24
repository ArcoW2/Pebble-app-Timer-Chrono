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

// Screen size is set once at startup from the window bounds, so the same
// code serves 200x228 (emery) and 144x168 (basalt, flint, diorite).
void    layout_set_screen(int16_t w, int16_t h);
int16_t layout_w(void);
int16_t layout_h(void);
int16_t layout_hint_w(void);   // right hint column
int16_t layout_corner(void);   // top-left Back hint

typedef struct {
  int16_t h;         // digit height
  int16_t w;         // digit cell width
  int16_t t;         // segment thickness
  int16_t narrow_w;  // ':' '.' cell width
  int16_t spacing;   // gap between cells
} DigitGeom;

typedef struct {
  uint8_t   size;         // the size actually used (may be a class smaller)
  int16_t   y, h;         // line box
  int16_t   gutter_w;     // indicator column
  int16_t   text_x;       // left edge of the digit text (right aligned)
  int16_t   text_w;
  int16_t   text_y;
  DigitGeom dg;           // normal: no overflow/sign cell drawn
  DigitGeom dg_sign;      // same height, narrower cells, sign cell shown
  int16_t   text_x_sign;
  int16_t   text_w_sign;
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
