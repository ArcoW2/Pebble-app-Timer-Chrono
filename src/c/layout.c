// layout.c - line boxes and digit geometry. Pure C, no pebble.h.
#include "layout.h"

#include <string.h>

#include "format.h"

static int16_t s_screen_w = 200;
static int16_t s_screen_h = 228;
static int16_t s_hint_w = 22;

void layout_set_screen(int16_t w, int16_t h) {
  s_screen_w = w;
  s_screen_h = h;
  s_hint_w = (w >= 180) ? 22 : 20;
}

int16_t layout_w(void) { return s_screen_w; }
int16_t layout_h(void) { return s_screen_h; }
int16_t layout_hint_w(void) { return s_hint_w; }
int16_t layout_corner(void) { return s_hint_w; }

//                                  L   M   S   XL
static const int16_t BOX_H[4]     = { 62, 42, 28, 96 };
static const int16_t GUTTER_W[4]  = { 30, 26, 18, 34 };
static const int16_t DIGIT_MAX[4] = { 54, 36, 22, 88 };
static const int16_t DIGIT_MIN[4] = { 26, 18, 14, 30 };
static const int16_t GAP[4]       = { 8, 6, 4, 10 };

// ==== HELPERS ====

static int16_t max16(int16_t a, int16_t b) { return a > b ? a : b; }
static int16_t min16(int16_t a, int16_t b) { return a < b ? a : b; }

static int16_t gap_between(uint8_t size_a, uint8_t size_b) {
  return max16(GAP[size_a], GAP[size_b]);
}

// ==== DIGIT FITTING ====

int16_t layout_text_width(uint8_t wide, uint8_t narrow, const DigitGeom *dg) {
  int16_t cells = wide + narrow;
  if (cells == 0) return 0;
  return wide * dg->w + narrow * dg->narrow_w + (cells - 1) * dg->spacing;
}

// Stroke weight belongs to the line's size class, not to whatever height
// the format leaves over: a line squeezed by a long format keeps its weight
// instead of turning spindly. Capped so a thin digit stays legible.
static int16_t stroke_for(int16_t class_h, int16_t h) {
  int16_t t = (class_h < 26) ? max16(3, class_h / 5) : max16(2, class_h / 8);
  int16_t cap = max16(2, h / 4);
  return min16(t, cap);
}

bool layout_fit_digits(uint8_t wide, uint8_t narrow, int16_t avail_w, int16_t max_h,
                       int16_t min_h, DigitGeom *out) {
  if (wide == 0) return false;
  for (int16_t h = max_h; h >= min_h; h--) {
    int16_t t = stroke_for(max_h, h);
    // A stroke spans its whole cell, so this spacing is the visible gap
    // between digits. Fixed at 1 px: it must not grow with the digit size.
    int16_t sp = 1;
    int16_t wmin = h * 36 / 100;   // narrower cells before giving up height
    int16_t wmax = h * 60 / 100;
    int16_t nw = max16(t + 2, h * 12 / 100);
    int16_t fixed = narrow * nw + (wide + narrow - 1) * sp;
    if (wide * wmin + fixed > avail_w) continue;
    out->h = h;
    out->t = t;
    out->spacing = sp;
    out->narrow_w = nw;
    out->w = min16(wmax, (avail_w - fixed) / wide);
    return true;
  }
  return false;
}

// ==== LAYOUT ====

// Size classes from tallest to shortest: a line that cannot hold its class
// steps down a whole class rather than keeping a tall box around small
// digits. -1 ends the chain.
static const int8_t NEXT_SMALLER[4] = { SIZE_M, SIZE_S, -1, SIZE_L };

// The size this line can actually carry, and its digit geometry.
static bool fit_line_size(uint8_t wanted, Format f, uint8_t *out_size, DigitGeom *dg) {
  uint8_t sz = wanted < SIZE_COUNT ? wanted : SIZE_M;
  uint8_t wide, narrow;
  format_counts_base(f, &wide, &narrow);  // the sign cell is usually blank
  for (;;) {
    int16_t avail = layout_w() - layout_hint_w() - GUTTER_W[sz] - 4;
    int8_t next = NEXT_SMALLER[sz];
    // Hold the class only if the digits stay taller than the next class's
    // ceiling; otherwise the smaller class is the honest description.
    int16_t floor_h = (next >= 0) ? (int16_t)(DIGIT_MAX[next] + 1) : DIGIT_MIN[sz];
    if (layout_fit_digits(wide, narrow, avail, DIGIT_MAX[sz], floor_h, dg)) {
      *out_size = sz;
      return true;
    }
    if (next < 0) return false;
    sz = (uint8_t)next;
  }
}

bool layout_compute(const ModeLayout *ml, const uint8_t *fmts, Layout *out) {
  memset(out, 0, sizeof(*out));
  out->n = ml->nlines;

  // Pass 1: the size each line can carry, from its width.
  for (uint8_t i = 0; i < ml->nlines; i++) {
    if (!fit_line_size(ml->lines[i].size, (Format)fmts[i], &out->g[i].size,
                       &out->g[i].dg)) {
      out->error = LAYOUT_TOO_WIDE;
      out->error_line = i;
      return false;
    }
  }


  // Pass 2: stack and centre. On a shorter screen the tallest line steps
  // down a class until the stack fits, rather than the layout being refused.
  int16_t total;
  for (;;) {
    total = 0;
    for (uint8_t i = 0; i < ml->nlines; i++) {
      total += BOX_H[out->g[i].size];
      if (i > 0) total += gap_between(out->g[i - 1].size, out->g[i].size);
    }
    if (total <= layout_h()) break;
    uint8_t tallest = 0;
    for (uint8_t i = 1; i < ml->nlines; i++) {
      if (BOX_H[out->g[i].size] > BOX_H[out->g[tallest].size]) tallest = i;
    }
    int8_t next = NEXT_SMALLER[out->g[tallest].size];
    if (next < 0) {
      out->error = LAYOUT_TOO_TALL;
      return false;
    }
    out->g[tallest].size = (uint8_t)next;
    uint8_t wide, narrow;
    format_counts_base((Format)fmts[tallest], &wide, &narrow);
    int16_t avail = layout_w() - layout_hint_w() - GUTTER_W[out->g[tallest].size] - 4;
    if (!layout_fit_digits(wide, narrow, avail, DIGIT_MAX[out->g[tallest].size],
                           DIGIT_MIN[out->g[tallest].size], &out->g[tallest].dg)) {
      out->error = LAYOUT_TOO_WIDE;
      out->error_line = tallest;
      return false;
    }
  }

  // Pass 1b: the same height with the overflow/sign cell added, by making
  // the cells narrower rather than by dropping a size.
  for (uint8_t i = 0; i < ml->nlines; i++) {
    LineGeom *g = &out->g[i];
    uint8_t wide, narrow;
    format_counts((Format)fmts[i], &wide, &narrow);
    int16_t avail = layout_w() - layout_hint_w() - GUTTER_W[g->size] - 4;
    g->dg_sign = g->dg;
    int16_t fixed = narrow * g->dg.narrow_w + (wide + narrow - 1) * g->dg.spacing;
    int16_t w = wide ? (avail - fixed) / wide : g->dg.w;
    int16_t wmin = max16(3, g->dg.h * 25 / 100);
    if (w < wmin) w = wmin;
    if (w > g->dg.w) w = g->dg.w;
    g->dg_sign.w = w;
  }

  int16_t y = (layout_h() - total) / 2;
  for (uint8_t i = 0; i < ml->nlines; i++) {
    LineGeom *g = &out->g[i];
    if (i > 0) y += gap_between(out->g[i - 1].size, g->size);
    g->y = y;
    g->h = BOX_H[g->size];
    g->gutter_w = GUTTER_W[g->size];

    uint8_t wide, narrow, wide_all;
    format_counts_base((Format)fmts[i], &wide, &narrow);
    format_counts((Format)fmts[i], &wide_all, &narrow);
    g->text_w = layout_text_width(wide, narrow, &g->dg);
    g->text_x = layout_w() - layout_hint_w() - 2 - g->text_w;
    g->text_w_sign = layout_text_width(wide_all, narrow, &g->dg_sign);
    g->text_x_sign = layout_w() - layout_hint_w() - 2 - g->text_w_sign;
    g->text_y = g->y + (g->h - g->dg.h) / 2;
    y += g->h;
  }
  out->error = LAYOUT_OK;
  return true;
}
