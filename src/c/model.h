// model.h - counter model. Pure C (no pebble.h), unit-tested on the host.
//
// A counter is data only: every displayed value is derived from `now`.
// Active time = wall time minus paused time. Laps store durations in
// active time, so pauses inside a lap are excluded by construction.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_LAPS 99

typedef enum {
  MODE_UP = 0,       // count up
  MODE_DOWN_FOR,     // count down for a duration
  MODE_DOWN_UNTIL,   // count down until a wall-clock end time (no pause)
  MODE_COUNT
} Mode;

typedef enum {
  ST_IDLE = 0,
  ST_RUNNING,
  ST_PAUSED,
  ST_STOPPED
} State;

typedef struct {
  uint8_t  mode;              // Mode
  uint8_t  state;             // State
  uint8_t  lap_stored;        // laps held in lap_ms[]
  uint8_t  until_day_offset;  // DOWN_UNTIL: extra days after next occurrence
  uint16_t until_clock_min;   // DOWN_UNTIL: end time, minutes of the day
  uint16_t lap_count;         // total laps, including rolled-off ones
  int64_t  start_ms;          // epoch of last (re)start
  int64_t  accum_ms;          // active time before last start
  int64_t  target_ms;         // DOWN_FOR: duration; DOWN_UNTIL: epoch end
  int64_t  offset_ms;         // UP: start offset (setup value)
  int64_t  last_lap_end_ms;   // active time at the last lap mark
  uint32_t lap_best_ms;
  uint32_t lap_worst_ms;
  uint64_t lap_sum_ms;
  uint32_t lap_ms[MAX_LAPS];  // lap durations, oldest first
} Counter;

// Size of the persisted header (everything before lap_ms).
#define COUNTER_HEADER_SIZE ((uint32_t)offsetof(Counter, lap_ms))

// ---- setup / lifecycle ----
void    counter_setup_up(Counter *c, int64_t offset_ms);
void    counter_setup_for(Counter *c, int64_t duration_ms);
void    counter_setup_until(Counter *c, uint16_t clock_min, uint8_t day_offset);
void    counter_reset(Counter *c);

// DOWN_UNTIL needs its epoch end, computed by the caller (local time).
void    counter_start(Counter *c, int64_t now, int64_t until_target_ms);
bool    counter_can_pause(const Counter *c);
void    counter_pause(Counter *c, int64_t now);
void    counter_resume(Counter *c, int64_t now);
void    counter_stop(Counter *c, int64_t now);
void    counter_stop_at_active(Counter *c, int64_t active_ms);

// ---- running edit ----
void    counter_set_duration(Counter *c, int64_t duration_ms);
void    counter_set_until(Counter *c, int64_t target_ms, uint16_t clock_min,
                          uint8_t day_offset);

// ---- values ----
int64_t counter_active_ms(const Counter *c, int64_t now);
int64_t counter_remaining_ms(const Counter *c, int64_t now);
bool    counter_lap(Counter *c, int64_t press_ms);
int64_t counter_current_lap_ms(const Counter *c, int64_t now);
bool    counter_lap_back(const Counter *c, uint8_t back, uint32_t *out);  // 1 = prior
bool    counter_best(const Counter *c, uint32_t *out);
bool    counter_worst(const Counter *c, uint32_t *out);
bool    counter_average(const Counter *c, uint32_t *out);
bool    counter_is_live(const Counter *c);  // running (values move)
