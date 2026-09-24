// draw.c - small drawing helpers.
#include "draw.h"

// ==== HELPERS ====

// One reusable path: points are set per call, nothing is allocated.
static GPath s_path;

void draw_fill_poly(GContext *ctx, GPoint *points, uint32_t count) {
  s_path.num_points = count;
  s_path.points = points;
  s_path.rotation = 0;
  s_path.offset = GPointZero;
  gpath_draw_filled(ctx, &s_path);
}

void draw_line_w(GContext *ctx, GPoint a, GPoint b, uint8_t width) {
  graphics_context_set_stroke_width(ctx, width < 1 ? 1 : width);
  graphics_draw_line(ctx, a, b);
}
