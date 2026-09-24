// model.c - counter model. Pure C, no pebble.h.
#include "model.h"

#include <string.h>

// ==== HELPERS ====

static void clear_laps(Counter *c) {
  c->lap_stored = 0;
  c->lap_count = 0;
  c->last_lap_end_ms = 0;
  c->lap_best_ms = 0;
  c->lap_worst_ms = 0;
  c->lap_sum_ms = 0;
  memset(c->lap_ms, 0, sizeof(c->lap_ms));
}

static void push_lap(Counter *c, uint32_t lap) {
  if (c->lap_stored == MAX_LAPS) {
    // Roll the oldest lap off; aggregates keep counting it.
    memmove(&c->lap_ms[0], &c->lap_ms[1], (MAX_LAPS - 1) * sizeof(uint32_t));
    c->lap_stored--;
  }
  c->lap_ms[c->lap_stored++] = lap;
  if (c->lap_count < UINT16_MAX) c->lap_count++;
  c->lap_sum_ms += lap;
  if (c->lap_count == 1 || lap < c->lap_best_ms) c->lap_best_ms = lap;
  if (c->lap_count == 1 || lap > c->lap_worst_ms) c->lap_worst_ms = lap;
}

// ==== SETUP / LIFECYCLE ====

void counter_setup_up(Counter *c, int64_t offset_ms) {
  c->mode = MODE_UP;
  c->offset_ms = offset_ms < 0 ? 0 : offset_ms;
  c->target_ms = 0;
  counter_reset(c);
}

void counter_setup_for(Counter *c, int64_t duration_ms) {
  c->mode = MODE_DOWN_FOR;
  c->target_ms = duration_ms < 0 ? 0 : duration_ms;
  c->offset_ms = 0;
  counter_reset(c);
}

void counter_setup_until(Counter *c, uint16_t clock_min, uint8_t day_offset) {
  c->mode = MODE_DOWN_UNTIL;
  c->until_clock_min = clock_min % (24 * 60);
  c->until_day_offset = day_offset;
  c->target_ms = 0;  // resolved at start
  c->offset_ms = 0;
  counter_reset(c);
}

void counter_reset(Counter *c) {
  c->state = ST_IDLE;
  c->start_ms = 0;
  c->accum_ms = (c->mode == MODE_UP) ? c->offset_ms : 0;
  clear_laps(c);
}

void counter_start(Counter *c, int64_t now, int64_t until_target_ms) {
  if (c->state != ST_IDLE) return;
  if (c->mode == MODE_DOWN_UNTIL) c->target_ms = until_target_ms;
  c->start_ms = now;
  c->state = ST_RUNNING;
}

bool counter_can_pause(const Counter *c) {
  return c->state == ST_RUNNING && c->mode != MODE_DOWN_UNTIL;
}

void counter_pause(Counter *c, int64_t now) {
  if (!counter_can_pause(c)) return;
  c->accum_ms += now - c->start_ms;
  c->state = ST_PAUSED;
}

void counter_resume(Counter *c, int64_t now) {
  if (c->state != ST_PAUSED) return;
  c->start_ms = now;
  c->state = ST_RUNNING;
}

void counter_stop(Counter *c, int64_t now) {
  if (c->state == ST_RUNNING) {
    c->accum_ms += now - c->start_ms;
  } else if (c->state != ST_PAUSED) {
    return;
  }
  c->state = ST_STOPPED;
}

void counter_stop_at_active(Counter *c, int64_t active_ms) {
  if (c->state != ST_RUNNING && c->state != ST_PAUSED) return;
  c->accum_ms = active_ms;
  c->state = ST_STOPPED;
}

// ==== RUNNING EDIT ====

void counter_set_duration(Counter *c, int64_t duration_ms) {
  if (c->mode != MODE_DOWN_FOR) return;
  c->target_ms = duration_ms < 0 ? 0 : duration_ms;
}

void counter_set_until(Counter *c, int64_t target_ms, uint16_t clock_min,
                       uint8_t day_offset) {
  if (c->mode != MODE_DOWN_UNTIL) return;
  c->target_ms = target_ms;
  c->until_clock_min = clock_min % (24 * 60);
  c->until_day_offset = day_offset;
}

// ==== VALUES ====

int64_t counter_active_ms(const Counter *c, int64_t now) {
  if (c->state == ST_RUNNING) return c->accum_ms + (now - c->start_ms);
  return c->accum_ms;
}

int64_t counter_remaining_ms(const Counter *c, int64_t now) {
  if (c->mode == MODE_DOWN_FOR) return c->target_ms - counter_active_ms(c, now);
  if (c->mode == MODE_DOWN_UNTIL) {
    if (c->state == ST_IDLE) return 0;  // end time not resolved yet
    // No pause in until-mode: active time equals wall time since start.
    return c->target_ms - (c->start_ms + counter_active_ms(c, now));
  }
  return 0;
}

bool counter_lap(Counter *c, int64_t press_ms) {
  if (c->state != ST_RUNNING && c->state != ST_PAUSED) return false;
  int64_t active = counter_active_ms(c, press_ms);
  int64_t lap = active - c->last_lap_end_ms;
  if (lap <= 0) return false;
  if (lap > (int64_t)UINT32_MAX) lap = UINT32_MAX;
  push_lap(c, (uint32_t)lap);
  c->last_lap_end_ms = active;
  return true;
}

int64_t counter_current_lap_ms(const Counter *c, int64_t now) {
  int64_t lap = counter_active_ms(c, now) - c->last_lap_end_ms;
  return lap < 0 ? 0 : lap;
}

bool counter_lap_back(const Counter *c, uint8_t back, uint32_t *out) {
  if (back == 0 || back > c->lap_stored) return false;
  *out = c->lap_ms[c->lap_stored - back];
  return true;
}

bool counter_best(const Counter *c, uint32_t *out) {
  if (c->lap_count == 0) return false;
  *out = c->lap_best_ms;
  return true;
}

bool counter_worst(const Counter *c, uint32_t *out) {
  if (c->lap_count == 0) return false;
  *out = c->lap_worst_ms;
  return true;
}

bool counter_average(const Counter *c, uint32_t *out) {
  if (c->lap_count == 0) return false;
  *out = (uint32_t)(c->lap_sum_ms / c->lap_count);
  return true;
}

bool counter_is_live(const Counter *c) {
  return c->state == ST_RUNNING;
}
