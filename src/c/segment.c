// segment.c - LCD-style seven-segment renderer.
//
// Digit cells: segments A-G. The sign cell adds a short centre vertical
// pair (for '+') and two diagonals (for '>' and '<'). Every digit in a line
// has the same width; ':' and '.' use narrow cells.
#include "segment.h"

#include "draw.h"

static int16_t max_i16(int16_t a, int16_t b) { return a > b ? a : b; }

enum {
  SEG_A = 1 << 0, SEG_B = 1 << 1, SEG_C = 1 << 2, SEG_D = 1 << 3,
  SEG_E = 1 << 4, SEG_F = 1 << 5, SEG_G = 1 << 6,
  SEG_PLUS_V = 1 << 7, SEG_DIAG_GT = 1 << 8, SEG_DIAG_LT = 1 << 9,
};

#define SEG_ALL_DIGIT 0x7F
#define SEG_ALL_SIGN  (SEG_G | SEG_PLUS_V)

// ==== GLYPHS ====

static uint16_t glyph_mask(char ch) {
  static const uint8_t DIGITS[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
  };
  if (ch >= '0' && ch <= '9') return DIGITS[ch - '0'];
  switch (ch) {
    case '-': return SEG_G;
    case '_': return SEG_D;
    case 'd': return SEG_B | SEG_C | SEG_D | SEG_E | SEG_G;
    case '+': return SEG_G | SEG_PLUS_V;
    case '>': return SEG_DIAG_GT;
    case '<': return SEG_DIAG_LT;
    default:  return 0;
  }
}

// ==== SEGMENT SHAPES ====

static void fill_hseg(GContext *ctx, int16_t x0, int16_t x1, int16_t y, int16_t t) {
  int16_t ht = t / 2 > 0 ? t / 2 : 1;
  GPoint p[6] = {
    { x0, y }, { x0 + ht, y - ht }, { x1 - ht, y - ht },
    { x1, y }, { x1 - ht, y + ht }, { x0 + ht, y + ht },
  };
  draw_fill_poly(ctx, p, 6);
}

static void fill_vseg(GContext *ctx, int16_t x, int16_t y0, int16_t y1, int16_t t) {
  int16_t ht = t / 2 > 0 ? t / 2 : 1;
  GPoint p[6] = {
    { x, y0 }, { x + ht, y0 + ht }, { x + ht, y1 - ht },
    { x, y1 }, { x - ht, y1 - ht }, { x - ht, y0 + ht },
  };
  draw_fill_poly(ctx, p, 6);
}

// Vertical strokes read heavier than horizontal ones at large sizes, so the
// biggest digits get slightly thinner verticals. Their outer edges stay on
// the cell boundary, which keeps the gap between digits at 1 px.
static int16_t vertical_thickness(int16_t t) {
  return (t >= 8) ? max_i16(2, t * 5 / 6) : t;
}

static void draw_segments(GContext *ctx, uint16_t mask, GRect cell, int16_t t,
                          int16_t seg_t) {
  int16_t l = cell.origin.x + t / 2;
  int16_t r = cell.origin.x + cell.size.w - 1 - t / 2;
  int16_t top = cell.origin.y + t / 2;
  int16_t mid = cell.origin.y + cell.size.h / 2;
  int16_t bot = cell.origin.y + cell.size.h - 1 - t / 2;
  // Gap between segments within a digit: fixed, so a big digit does not
  // turn into scattered strokes.
  const int16_t g = 1;
  int16_t cx = cell.origin.x + cell.size.w / 2;
  int16_t vt = vertical_thickness(seg_t);
  int16_t lv = cell.origin.x + vt / 2;
  int16_t rv = cell.origin.x + cell.size.w - 1 - vt / 2;

  // Segment centres stay put; only the drawn thickness varies, so a ghost
  // sits exactly where its lit counterpart would.
  if (mask & SEG_A) fill_hseg(ctx, l + g, r - g, top, seg_t);
  if (mask & SEG_G) fill_hseg(ctx, l + g, r - g, mid, seg_t);
  if (mask & SEG_D) fill_hseg(ctx, l + g, r - g, bot, seg_t);
  if (mask & SEG_F) fill_vseg(ctx, lv, top + g, mid - g, vt);
  if (mask & SEG_B) fill_vseg(ctx, rv, top + g, mid - g, vt);
  if (mask & SEG_E) fill_vseg(ctx, lv, mid + g, bot - g, vt);
  if (mask & SEG_C) fill_vseg(ctx, rv, mid + g, bot - g, vt);
  if (mask & SEG_PLUS_V) {
    int16_t reach = cell.size.h / 3;  // short verticals read as a lump
    fill_vseg(ctx, cx, mid - reach, mid - g, vt);
    fill_vseg(ctx, cx, mid + g, mid + reach, vt);
  }
  if (mask & (SEG_DIAG_GT | SEG_DIAG_LT)) {
    int16_t inset = cell.size.h / 6;
    bool gt = mask & SEG_DIAG_GT;
    GPoint tip = GPoint(gt ? r : l, mid);
    GPoint a = GPoint(gt ? l : r, top + inset);
    GPoint b = GPoint(gt ? l : r, bot - inset);
    draw_line_w(ctx, a, tip, (uint8_t)seg_t);
    draw_line_w(ctx, tip, b, (uint8_t)seg_t);
  }
}

// Ghost segments have the same weight as lit ones; the level decides their
// colour, not their thickness.
static int16_t ghost_thickness(int16_t t, uint8_t level) {
  return level ? t : 0;
}

static void draw_separator(GContext *ctx, char ch, GRect cell, int16_t t) {
  int16_t s = t;
  int16_t cx = cell.origin.x + cell.size.w / 2 - s / 2;
  if (ch == ':') {
    int16_t y1 = cell.origin.y + cell.size.h * 3 / 10 - s / 2;
    int16_t y2 = cell.origin.y + cell.size.h * 7 / 10 - s / 2;
    graphics_fill_rect(ctx, GRect(cx, y1, s, s), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(cx, y2, s, s), 0, GCornerNone);
  } else if (ch == '.') {
    int16_t y = cell.origin.y + cell.size.h - s;
    graphics_fill_rect(ctx, GRect(cx, y, s, s), 0, GCornerNone);
  }
}

// ==== PUBLIC ====

void segment_draw_text(GContext *ctx, const LineText *text, GPoint origin,
                       const DigitGeom *dg, const SegColors *colors) {
  graphics_context_set_antialiased(ctx, true);
  int16_t x = origin.x;
  for (uint8_t i = 0; i < text->n; i++) {
    const Cell *c = &text->cells[i];
    bool sep = (c->kind == CELL_SEP);
    int16_t w = sep ? dg->narrow_w : dg->w;
    GRect cell = GRect(x, origin.y, w, dg->h);
    bool ghost_only = c->flags & CF_GHOST;
    GColor lit = (c->flags & CF_DIM) ? colors->dim : colors->lit;
    int16_t gt = ghost_thickness(dg->t, colors->ghost_level);

    if (sep) {
      if (ghost_only) {
        if (gt > 0) {
          graphics_context_set_fill_color(ctx, colors->ghost);
          draw_separator(ctx, c->ch, cell, gt);
        }
      } else {
        graphics_context_set_fill_color(ctx, lit);
        draw_separator(ctx, c->ch, cell, dg->t);
      }
    } else {
      uint16_t all = (c->kind == CELL_SIGN) ? SEG_ALL_SIGN : SEG_ALL_DIGIT;
      uint16_t on = ghost_only ? 0 : glyph_mask(c->ch);
      if (gt > 0) {  // level 0: unlit segments are simply not drawn
        graphics_context_set_fill_color(ctx, colors->ghost);
        graphics_context_set_stroke_color(ctx, colors->ghost);
        draw_segments(ctx, all & ~on, cell, dg->t, gt);
      }
      if (on) {
        graphics_context_set_fill_color(ctx, lit);
        graphics_context_set_stroke_color(ctx, lit);
        draw_segments(ctx, on, cell, dg->t, dg->t);
      }
    }
    x += w + dg->spacing;
  }
}
