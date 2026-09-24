// draw.h - small drawing helpers shared by the segment renderer and icons.
#pragma once

#include <pebble.h>

void draw_fill_poly(GContext *ctx, GPoint *points, uint32_t count);
void draw_line_w(GContext *ctx, GPoint a, GPoint b, uint8_t width);
