// settings.h - settings definitions. Pure C (no pebble.h).
//
// PhoneSettings: edited on the phone config page (layouts, colours, reveal
//                timeout), received once on close, stored on the watch.
// WatchSettings: edited on the watch (thresholds, alarms, output, lap feedback).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "model.h"

// ---- line sources ----
typedef enum {
  SRC_NONE = 0,
  SRC_TOTAL,          // running: total active time
  SRC_REMAINING,      // running: count down only
  SRC_CUR_LAP,        // running: current lap
  SRC_PRIOR,          // static: prior lap
  SRC_PRIOR2,         // static: lap before prior
  SRC_BEST,           // static: fastest lap
  SRC_AVG,            // static: average lap
  SRC_DELTA_PRIOR,    // running: current lap minus prior lap
  SRC_DELTA_BEST,     // running: current lap minus fastest lap
  SRC_DELTA_AVG,      // running: current lap minus average lap
  SRC_UNTIL,          // static: end time (until-mode)
  SRC_LAPCOUNT,       // static: number of laps
  SRC_COUNT
} Source;

typedef enum { SIZE_L = 0, SIZE_M, SIZE_S, SIZE_COUNT } LineSize;

// Formats. AUTO resolves per source (see lines.c). The last three are
// fixed formats that belong to one source type.
typedef enum {
  FMT_AUTO = 0,
  FMT_DHMS,     // 99d23:59:59
  FMT_HMS,      // 99:59:59
  FMT_MS,       // 59:59
  FMT_MST,      // 59:59.9
  FMT_DELTA,    // +9:59
  FMT_HM,       // 23:59 (clock time)
  FMT_COUNT2,   // 99
  FMT_COUNT
} Format;

#define LINE_FLAG_THRESHOLDS 0x01
#define MAX_LINES 4

typedef struct {
  uint8_t source;
  uint8_t size;
  uint8_t format;
  uint8_t flags;
} LineCfg;

typedef struct {
  uint8_t fg;       // palette index
  uint8_t bg;
  uint8_t accent;
  uint8_t behind;   // overrun and "slower than the reference"
  uint8_t ahead;    // "faster than the reference"
  uint8_t nlines;
  LineCfg lines[MAX_LINES];
} ModeLayout;

#define PHONE_SETTINGS_VERSION 2  // bump whenever PhoneSettings changes shape

#define STYLE_GHOSTING 0x01

typedef struct {
  uint8_t    version;
  uint8_t    reveal_s;
  uint8_t    style;     // STYLE_* flags
  ModeLayout mode[MODE_COUNT];
} PhoneSettings;

// Phone config wire format: [version, reveal_s, style, then per mode:
// fg, bg, accent, behind, ahead, nlines, 4 x (source, size, format, flags)].
#define PHONE_CFG_HEADER 3
#define PHONE_CFG_MODE (6 + MAX_LINES * 4)
#define PHONE_CFG_BYTES (PHONE_CFG_HEADER + MODE_COUNT * PHONE_CFG_MODE)

// ---- watch settings ----
typedef enum { OUT_OFF = 0, OUT_VIBE, OUT_SOUND, OUT_BOTH, OUT_COUNT } Output;
typedef enum { REF_OFF = 0, REF_PRIOR, REF_FIXED, REF_BEST, REF_AVG, REF_COUNT } LapRef;
typedef enum { ZERO_OVERRUN = 0, ZERO_STOP, ZERO_COUNT } AtZero;

typedef struct {
  // count up
  uint32_t lap_fixed_ms;
  uint32_t prewarn_ms;     // 0 = off
  uint32_t total_ms;       // 0 = off
  // count down
  uint32_t interval_ms;    // 0 = off
  uint32_t preend_ms;      // 0 = off
  uint8_t  lap_ref;        // LapRef (count up)
  uint8_t  lap_out;        // Output
  uint8_t  prewarn_out;
  uint8_t  total_out;
  uint8_t  interval_out;
  uint8_t  preend_out;
  uint8_t  end_out;
  uint8_t  at_zero;        // AtZero
  uint8_t  lapfb_out;      // lap press feedback
} AlarmCfg;

#define WATCH_SETTINGS_VERSION 1

typedef struct {
  uint8_t  version;
  uint32_t thr_coarse_ms;  // above: 1 min resolution
  uint32_t thr_fine_ms;    // above: 10 s; below: finest
  AlarmCfg mode[MODE_COUNT];
} WatchSettings;

void settings_phone_defaults(PhoneSettings *p);
void settings_watch_defaults(WatchSettings *w);

// Decodes and sanitizes the phone wire format. Invalid enum values are
// replaced by defaults. Returns false if the blob is unusable.
bool settings_phone_decode(const uint8_t *data, uint16_t len, PhoneSettings *out);

// Default layout for one mode (used for defaults and fallbacks).
void settings_mode_layout_default(Mode m, ModeLayout *out);
