// layout.c - line boxes and digit geometry. Pure C, no pebble.h.
#include "layout.h"

#include <string.h>

#include "format.h"

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

bool layout_fit_digits(uint8_t wide, uint8_t narrow, int16_t avail_w, int16_t max_h,
                       int16_t min_h, DigitGeom *out) {
  if (wide == 0) return false;
  for (int16_t h = max_h; h >= min_h; h--) {
    // Small digits need proportionally thicker segments to stay readable.
    int16_t t = max16(3, h < 26 ? h / 5 : (h < 44 ? h / 6 : h / 7));
    int16_t sp = max16(1, t / 3);
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

bool layout_compute(const ModeLayout *ml, const uint8_t *fmts, Layout *out) {
  memset(out, 0, sizeof(*out));
  out->n = ml->nlines;

  int16_t total = 0;
  for (uint8_t i = 0; i < ml->nlines; i++) {
    uint8_t sz = ml->lines[i].size;
    total += BOX_H[sz];
    if (i > 0) total += gap_between(ml->lines[i - 1].size, sz);
  }
  if (total > SCREEN_H) {
    out->error = LAYOUT_TOO_TALL;
    return false;
  }

  int16_t y = (SCREEN_H - total) / 2;
  for (uint8_t i = 0; i < ml->nlines; i++) {
    uint8_t sz = ml->lines[i].size;
    LineGeom *g = &out->g[i];
    if (i > 0) y += gap_between(ml->lines[i - 1].size, sz);
    g->y = y;
    g->h = BOX_H[sz];
    g->gutter_w = GUTTER_W[sz];

    uint8_t wide, narrow;
    format_counts((Format)fmts[i], &wide, &narrow);
    int16_t avail = SCREEN_W - HINT_W - g->gutter_w - 4;
    if (!layout_fit_digits(wide, narrow, avail, DIGIT_MAX[sz], DIGIT_MIN[sz], &g->dg)) {
      out->error = LAYOUT_TOO_WIDE;
      out->error_line = i;
      return false;
    }
    g->text_w = layout_text_width(wide, narrow, &g->dg);
    g->text_x = SCREEN_W - HINT_W - 2 - g->text_w;
    g->text_y = g->y + (g->h - g->dg.h) / 2;
    y += g->h;
  }
  out->error = LAYOUT_OK;
  return true;
}
