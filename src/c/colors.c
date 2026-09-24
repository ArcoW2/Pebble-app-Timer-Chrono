// colors.c - palette indices from settings to GColor, per mode.
#include "colors.h"

#include "app.h"
#include "format.h"

// Palette order matches settings.c and the phone config page.
static const uint8_t PALETTE_ARGB8[] = {
  GColorBlackARGB8,          // 0
  GColorWhiteARGB8,          // 1
  GColorOxfordBlueARGB8,     // 2
  GColorDarkGrayARGB8,       // 3
  GColorDarkGreenARGB8,      // 4
  GColorImperialPurpleARGB8, // 5
  GColorYellowARGB8,         // 6
  GColorElectricBlueARGB8,   // 7
  GColorLightGrayARGB8,      // 8
  GColorChromeYellowARGB8,   // 9
  GColorOrangeARGB8,         // 10
  GColorVividCeruleanARGB8,  // 11
  GColorShockingPinkARGB8,   // 12
};
#define PALETTE_COUNT (sizeof(PALETTE_ARGB8) / sizeof(PALETTE_ARGB8[0]))

// ==== HELPERS ====

static GColor palette_color(uint8_t index, uint8_t fallback) {
  uint8_t i = index < PALETTE_COUNT ? index : fallback;
  return (GColor){ .argb = PALETTE_ARGB8[i] };
}

static bool is_light(uint8_t index) {
  return index == 1 || index == 6 || index == 7 || index == 8 || index == 9;
}

// Ghost segments: a step away from the background, subtle but visible.
static GColor ghost_for_bg(uint8_t bg) {
  switch (bg) {
    case 1:  return GColorLightGray;      // white
    case 2:  return GColorDukeBlue;       // oxford blue
    case 3:  return GColorBlack;          // dark gray
    case 4:  return GColorMidnightGreen;  // dark green
    case 5:  return GColorBlack;          // imperial purple
    default: return GColorDarkGray;       // black
  }
}

// ==== PUBLIC ====

void colors_for_mode(uint8_t mode, Palette *out) {
  const ModeLayout *ml = &g_phone.mode[mode < MODE_COUNT ? mode : 0];
  uint8_t bg = ml->bg, fg = ml->fg;
  if (fg == bg || is_light(fg) == is_light(bg)) fg = is_light(bg) ? 0 : 1;  // contrast
  bool light = is_light(bg);
  out->bg = palette_color(bg, 0);
  out->fg = palette_color(fg, 1);
  out->accent = palette_color(ml->accent, 10);
  out->ghost = ghost_for_bg(bg);
  out->dim = light ? GColorDarkGray : GColorLightGray;
  out->red = light ? GColorDarkCandyAppleRed : GColorRed;
  out->green = light ? GColorIslamicGreen : GColorGreen;
}

GColor colors_for_class(const Palette *p, uint8_t color_class) {
  switch (color_class) {
    case COL_RED:   return p->red;
    case COL_GREEN: return p->green;
    default:        return p->fg;
  }
}
