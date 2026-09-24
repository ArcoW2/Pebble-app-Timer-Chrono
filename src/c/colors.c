// colors.c - palette indices from settings to GColor, per mode.
#include "colors.h"

#include "app.h"
#include "format.h"

#ifndef PBL_BW
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
  GColorRedARGB8,            // 13
  GColorGreenARGB8,          // 14
  GColorDarkCandyAppleRedARGB8,  // 15
  GColorIslamicGreenARGB8,   // 16
  GColorFollyARGB8,          // 17
  GColorBulgarianRoseARGB8,  // 18
};
#define PALETTE_COUNT (sizeof(PALETTE_ARGB8) / sizeof(PALETTE_ARGB8[0]))

// ==== HELPERS ====

static GColor palette_color(uint8_t index, uint8_t fallback) {
  uint8_t i = index < PALETTE_COUNT ? index : fallback;
  return (GColor){ .argb = PALETTE_ARGB8[i] };
}

static bool is_light(uint8_t index) {
  return index == 1 || index == 6 || index == 7 || index == 8 || index == 9 ||
         index == 13 || index == 14 || index == 17;
}

// Ghost colour: with w = STYLE_GHOST_MAX - level, each channel becomes
// (w * background + foreground) / (w + 1). A higher level means less
// background weight, so the ghost moves towards the digit colour.
static GColor blend_toward_bg(GColor bg, GColor fg, uint8_t level) {
  uint8_t w = (level >= STYLE_GHOST_MAX) ? 0 : (uint8_t)(STYLE_GHOST_MAX - level);
  uint8_t out = 0;
  for (int shift = 0; shift <= 4; shift += 2) {      // blue, green, red
    uint8_t b = (bg.argb >> shift) & 0x03;
    uint8_t f = (fg.argb >> shift) & 0x03;
    uint8_t v = (uint8_t)((b * w + f + w / 2) / (w + 1));
    out |= (uint8_t)((v & 0x03) << shift);
  }
  return (GColor){ .argb = (uint8_t)(0xC0 | out) };  // keep it opaque
}

#endif  // !PBL_BW

// ==== PUBLIC ====

void colors_for_mode(uint8_t mode, Palette *out) {
#ifdef PBL_BW
  // One bit per pixel: black on white, no ghosts, no delta colours.
  (void)mode;
  out->bg = GColorWhite;
  out->fg = GColorBlack;
  out->accent = GColorBlack;
  out->ghost = GColorWhite;
  out->dim = GColorBlack;
  out->red = GColorBlack;
  out->green = GColorBlack;
  return;
#else
  const ModeLayout *ml = &g_phone.mode[mode < MODE_COUNT ? mode : 0];
  uint8_t bg = ml->bg, fg = ml->fg;
  // No correction: whatever the user picked is what gets drawn.
  bool light = is_light(bg);
  out->bg = palette_color(bg, 0);
  out->fg = palette_color(fg, 1);
  out->accent = palette_color(ml->accent, 10);
  uint8_t level = STYLE_GHOST_LEVEL(g_phone.style);
  out->ghost = level ? blend_toward_bg(out->bg, out->fg, level) : out->bg;
  out->dim = light ? GColorDarkGray : GColorLightGray;
  // Behind / ahead are chosen per mode, so they can stay readable on a
  // reddish or greenish background.
  out->red = palette_color(ml->behind, light ? 15 : 13);
  out->green = palette_color(ml->ahead, light ? 16 : 14);
#endif
}

GColor colors_for_class(const Palette *p, uint8_t color_class) {
  switch (color_class) {
    case COL_RED:   return p->red;
    case COL_GREEN: return p->green;
    default:        return p->fg;
  }
}
