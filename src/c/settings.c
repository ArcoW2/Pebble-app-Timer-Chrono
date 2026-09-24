// settings.c - defaults and phone-config decoding. Pure C, no pebble.h.
#include "settings.h"

#include <string.h>

// Palette indices (see colors.c): 0 Black, 1 White, 2 Oxford blue,
// 3 Dark gray, 4 Dark green, 5 Imperial purple, 6 Yellow, 7 Electric blue,
// 8 Light gray, 9 Chrome yellow, 10 Orange, 11 Vivid cerulean,
// 12 Shocking pink, 13 Red, 14 Green, 15 Dark red, 16 Islamic green,
// 17 Folly, 18 Bulgarian rose.
#define PALETTE_COUNT 19

// ==== HELPERS ====

static LineCfg line(uint8_t source, uint8_t size, uint8_t format, uint8_t flags) {
  LineCfg l = { source, size, format, flags };
  return l;
}

static uint8_t clamp_enum(uint8_t v, uint8_t count, uint8_t fallback) {
  return v < count ? v : fallback;
}

// ==== DEFAULTS ====

void settings_mode_layout_default(Mode m, ModeLayout *out) {
  memset(out, 0, sizeof(*out));
  switch (m) {
    case MODE_UP:
      out->fg = 1; out->bg = 0; out->accent = 10;
      out->behind = 13; out->ahead = 14;
      out->nlines = 3;
      out->lines[0] = line(SRC_TOTAL, SIZE_L, FMT_AUTO, LINE_FLAG_THRESHOLDS);
      out->lines[1] = line(SRC_CUR_LAP, SIZE_M, FMT_MS, LINE_FLAG_THRESHOLDS);
      out->lines[2] = line(SRC_DELTA_BEST, SIZE_S, FMT_DELTA, 0);
      break;
    case MODE_DOWN_FOR:
      out->fg = 1; out->bg = 2; out->accent = 7;
      out->behind = 13; out->ahead = 14;
      out->nlines = 3;
      out->lines[0] = line(SRC_REMAINING, SIZE_L, FMT_AUTO, LINE_FLAG_THRESHOLDS);
      out->lines[1] = line(SRC_CUR_LAP, SIZE_S, FMT_MS, LINE_FLAG_THRESHOLDS);
      out->lines[2] = line(SRC_TOTAL, SIZE_S, FMT_AUTO, LINE_FLAG_THRESHOLDS);
      break;
    default:  // MODE_DOWN_UNTIL
      out->fg = 9; out->bg = 0; out->accent = 11;
      out->behind = 13; out->ahead = 14;
      out->nlines = 3;
      out->lines[0] = line(SRC_REMAINING, SIZE_L, FMT_AUTO, LINE_FLAG_THRESHOLDS);
      out->lines[1] = line(SRC_CUR_LAP, SIZE_S, FMT_MS, LINE_FLAG_THRESHOLDS);
      out->lines[2] = line(SRC_UNTIL, SIZE_S, FMT_HM, 0);
      break;
  }
}

void settings_phone_defaults(PhoneSettings *p) {
  memset(p, 0, sizeof(*p));
  p->version = PHONE_SETTINGS_VERSION;
  p->reveal_s = 5;
  p->style = 6;  // a little above the background
  for (int m = 0; m < MODE_COUNT; m++) {
    settings_mode_layout_default((Mode)m, &p->mode[m]);
  }
}

void settings_watch_defaults(WatchSettings *w) {
  memset(w, 0, sizeof(*w));
  w->version = WATCH_SETTINGS_VERSION;
  w->thr_coarse_ms = 10 * 60 * 1000;
  w->thr_fine_ms = 60 * 1000;
  w->click_out = OUT_VIBE;
  for (int m = 0; m < MODE_COUNT; m++) {
    AlarmCfg *a = &w->mode[m];
    a->lap_ref = REF_OFF;
    a->lap_fixed_ms = 60 * 1000;
    a->lap_out = OUT_VIBE;
    a->prewarn_out = OUT_VIBE;
    a->total_out = OUT_VIBE;
    a->interval_out = OUT_VIBE;
    a->preend_out = OUT_VIBE;
    a->end_out = OUT_VIBE;
    a->at_zero = ZERO_OVERRUN;
    a->lapfb_out = OUT_VIBE;
  }
}

// ==== PHONE DECODE ====

static void sanitize_line(LineCfg *l) {
  l->source = clamp_enum(l->source, SRC_COUNT, SRC_NONE);
  l->size = clamp_enum(l->size, SIZE_COUNT, SIZE_M);
  l->format = clamp_enum(l->format, FMT_COUNT, FMT_AUTO);
  l->flags &= LINE_FLAG_THRESHOLDS;
}

static void decode_mode(const uint8_t *d, ModeLayout *out, Mode m) {
  ModeLayout def;
  settings_mode_layout_default(m, &def);
  out->fg = clamp_enum(d[0], PALETTE_COUNT, def.fg);
  out->bg = clamp_enum(d[1], PALETTE_COUNT, def.bg);
  out->accent = clamp_enum(d[2], PALETTE_COUNT, def.accent);
  out->behind = clamp_enum(d[3], PALETTE_COUNT, def.behind);
  out->ahead = clamp_enum(d[4], PALETTE_COUNT, def.ahead);
  out->nlines = 0;
  for (int i = 0; i < MAX_LINES; i++) {
    LineCfg l = line(d[6 + i * 4], d[7 + i * 4], d[8 + i * 4], d[9 + i * 4]);
    sanitize_line(&l);
    if (l.source != SRC_NONE) out->lines[out->nlines++] = l;  // compact
  }
  for (int i = out->nlines; i < MAX_LINES; i++) out->lines[i] = line(0, 0, 0, 0);
  if (out->nlines == 0) *out = def;  // an empty layout is never useful
}

bool settings_phone_decode(const uint8_t *data, uint16_t len, PhoneSettings *out) {
  if (!data || len < PHONE_CFG_BYTES) return false;
  if (data[0] != PHONE_SETTINGS_VERSION) return false;
  out->version = PHONE_SETTINGS_VERSION;
  out->reveal_s = data[1] < 1 ? 1 : (data[1] > 30 ? 30 : data[1]);
  out->style = data[2] & STYLE_GHOST_MASK;
  for (int m = 0; m < MODE_COUNT; m++) {
    decode_mode(&data[PHONE_CFG_HEADER + m * PHONE_CFG_MODE], &out->mode[m], (Mode)m);
  }
  return true;
}
