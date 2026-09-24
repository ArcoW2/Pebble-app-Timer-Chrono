// alarm_calc.h - when is the next alarm? Pure C (no pebble.h).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "model.h"
#include "settings.h"

typedef enum {
  AL_NONE = 0,
  AL_LAP,        // count up: current lap reaches the reference
  AL_PREWARN,    // count up: X before the lap reference
  AL_TOTAL,      // count up: total reaches a fixed time
  AL_INTERVAL,   // count down: every X, aligned to remaining time
  AL_PREEND,     // count down: X before the end
  AL_END,        // count down: zero
  AL_COUNT
} AlarmType;

typedef struct {
  int64_t at_ms;  // epoch
  uint8_t type;   // AlarmType
} AlarmEvent;

// Reference lap for count-up lap alarms (prior, fixed, best, average).
bool alarm_lap_reference(const Counter *c, const AlarmCfg *cfg, uint32_t *out);

// Output configured for an alarm type.
uint8_t alarm_output(const AlarmCfg *cfg, uint8_t type);

// Earliest alarm strictly after `now`, for a running counter. Alarms whose
// output is off are skipped, except END when "stop at zero" needs it.
bool alarm_next(const Counter *c, const AlarmCfg *cfg, int64_t now, AlarmEvent *out);
