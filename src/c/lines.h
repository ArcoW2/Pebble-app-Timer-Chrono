// lines.h - value, format and refresh data per line. Pure C (no pebble.h).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "format.h"
#include "model.h"
#include "settings.h"

typedef struct {
  bool    has;            // false: show dashes
  int64_t value;          // ms (or a count for SRC_LAPCOUNT)
  int8_t  rate;           // +1 rising, -1 falling, 0 static
  uint8_t color;          // ColClass
  int64_t measured;       // for adaptive resolution
  int8_t  measured_rate;
} LineValue;

bool    lines_source_is_running(uint8_t source);
// AUTO drops the fields that are not in use, so the digits stay as large as
// the line allows. Re-resolved as the value crosses a day/hour boundary.
uint8_t lines_resolve_format(uint8_t source, uint8_t format, const Counter *c,
                             int64_t now);
// Widest format a line can resolve to in a mode (layout validation).
uint8_t lines_worst_format(uint8_t source, uint8_t format, Mode mode);
int64_t lines_finest_res(uint8_t format);

void lines_value(const Counter *c, const AlarmCfg *cfg, uint8_t source, int64_t now,
                 LineValue *out);

// Display resolution for a line right now.
int64_t lines_resolution(const Counter *c, const LineCfg *lc, uint8_t fmt,
                         const LineValue *v, const WatchSettings *ws, bool reveal);

// ms until this line's text changes (value or resolution). <0 = never.
int64_t lines_next_change_ms(const LineCfg *lc, const LineValue *v, int64_t res,
                             const WatchSettings *ws, bool reveal, bool live);
