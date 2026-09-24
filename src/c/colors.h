// colors.h - palette indices from settings to GColor, per mode.
#pragma once

#include <pebble.h>

typedef struct {
  GColor fg, bg, accent;
  GColor ghost;   // unlit LCD segments
  GColor dim;     // unselected editor fields
  GColor red, green;
} Palette;

void   colors_for_mode(uint8_t mode, Palette *out);
GColor colors_for_class(const Palette *p, uint8_t color_class);
