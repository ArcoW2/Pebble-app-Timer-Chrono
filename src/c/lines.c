// lines.c - value, format and refresh data per line. Pure C, no pebble.h.
#include "lines.h"

#include <string.h>

#include "alarm_calc.h"

// ==== HELPERS ====

static bool is_time_source(uint8_t s) {
  return s == SRC_TOTAL || s == SRC_REMAINING;
}

static bool is_lap_source(uint8_t s) {
  return s == SRC_CUR_LAP || s == SRC_PRIOR || s == SRC_PRIOR2 || s == SRC_BEST ||
         s == SRC_AVG;
}

static bool is_delta_source(uint8_t s) {
  return s == SRC_DELTA_PRIOR || s == SRC_DELTA_BEST || s == SRC_DELTA_AVG;
}

static bool format_allowed(uint8_t source, uint8_t f) {
  if (is_time_source(source)) {
    return f == FMT_DHMS || f == FMT_HMS || f == FMT_MS || f == FMT_MST;
  }
  if (is_lap_source(source)) return f == FMT_HMS || f == FMT_MS || f == FMT_MST;
  return false;  // delta, until, lap count have one fixed format
}

static uint8_t down_time_format(const Counter *c) {
  if (c->mode == MODE_DOWN_UNTIL) return c->until_day_offset > 0 ? FMT_DHMS : FMT_HMS;
  if (c->target_ms >= 86400000LL) return FMT_DHMS;
  if (c->target_ms >= 3600000LL) return FMT_HMS;
  return FMT_MS;
}

static uint8_t fixed_format(uint8_t source) {
  if (is_delta_source(source)) return FMT_DELTA;
  if (source == SRC_UNTIL) return FMT_HM;
  if (source == SRC_LAPCOUNT) return FMT_COUNT2;
  return FMT_MS;
}

static int64_t min_pos(int64_t a, int64_t b) {
  if (a < 0) return b;
  if (b < 0) return a;
  return a < b ? a : b;
}

static bool delta_reference(const Counter *c, uint8_t source, uint32_t *ref) {
  switch (source) {
    case SRC_DELTA_PRIOR: return counter_lap_back(c, 1, ref);
    case SRC_DELTA_BEST:  return counter_best(c, ref);
    case SRC_DELTA_AVG:   return counter_average(c, ref);
    default:              return false;
  }
}

// ==== FORMATS ====

bool lines_source_is_running(uint8_t s) {
  return is_time_source(s) || s == SRC_CUR_LAP || is_delta_source(s);
}

uint8_t lines_resolve_format(uint8_t source, uint8_t format, const Counter *c) {
  if (format != FMT_AUTO && format_allowed(source, format)) return format;
  if (is_time_source(source)) {
    return (c->mode == MODE_UP) ? FMT_DHMS : down_time_format(c);
  }
  return fixed_format(source);
}

uint8_t lines_worst_format(uint8_t source, uint8_t format, Mode mode) {
  (void)mode;
  if (format != FMT_AUTO && format_allowed(source, format)) return format;
  if (is_time_source(source)) return FMT_DHMS;
  return fixed_format(source);
}

int64_t lines_finest_res(uint8_t format) {
  return format == FMT_MST ? 100 : 1000;
}

// ==== VALUES ====

static void set_measured(const Counter *c, const AlarmCfg *cfg, uint8_t source,
                         int64_t now, LineValue *o) {
  if (c->state != ST_RUNNING) return;
  if (is_delta_source(source)) {
    o->measured = o->value < 0 ? -o->value : o->value;
    o->measured_rate = o->value < 0 ? -1 : +1;
    return;
  }
  if (c->mode == MODE_UP) {
    o->measured = o->value;  // count up: the line's own running value
    o->measured_rate = +1;
    return;
  }
  // Count down: time to the next event (end or alarm).
  int64_t rem = counter_remaining_ms(c, now);
  o->measured = rem > 0 ? rem : -rem;
  o->measured_rate = rem > 0 ? -1 : +1;
  AlarmEvent e;
  if (alarm_next(c, cfg, now, &e) && e.at_ms - now < o->measured) {
    o->measured = e.at_ms - now;
    o->measured_rate = -1;
  }
}

void lines_value(const Counter *c, const AlarmCfg *cfg, uint8_t source, int64_t now,
                 LineValue *o) {
  memset(o, 0, sizeof(*o));
  o->color = COL_FG;
  bool live = (c->state == ST_RUNNING);
  uint32_t u = 0;

  switch (source) {
    case SRC_TOTAL:
      o->has = true;
      o->value = counter_active_ms(c, now);
      o->rate = live ? +1 : 0;
      break;
    case SRC_REMAINING:
      if (c->mode == MODE_UP) break;
      if (c->mode == MODE_DOWN_UNTIL && c->state == ST_IDLE) break;
      o->has = true;
      o->value = counter_remaining_ms(c, now);
      o->rate = live ? -1 : 0;
      if (o->value < 0) o->color = COL_RED;  // overrun
      break;
    case SRC_CUR_LAP:
      o->has = true;
      o->value = counter_current_lap_ms(c, now);
      o->rate = live ? +1 : 0;
      break;
    case SRC_PRIOR:  o->has = counter_lap_back(c, 1, &u); o->value = u; break;
    case SRC_PRIOR2: o->has = counter_lap_back(c, 2, &u); o->value = u; break;
    case SRC_BEST:   o->has = counter_best(c, &u);        o->value = u; break;
    case SRC_AVG:    o->has = counter_average(c, &u);     o->value = u; break;
    case SRC_DELTA_PRIOR:
    case SRC_DELTA_BEST:
    case SRC_DELTA_AVG:
      if (!delta_reference(c, source, &u)) break;
      o->has = true;
      o->value = counter_current_lap_ms(c, now) - (int64_t)u;
      o->rate = live ? +1 : 0;
      o->color = o->value < 0 ? COL_GREEN : COL_RED;
      break;
    case SRC_UNTIL:
      o->has = (c->mode == MODE_DOWN_UNTIL);
      o->value = (int64_t)c->until_clock_min * 60000LL;
      break;
    case SRC_LAPCOUNT:
      o->has = true;
      o->value = c->lap_count;
      break;
    default:
      break;
  }
  if (o->has && lines_source_is_running(source)) set_measured(c, cfg, source, now, o);
}

// ==== RESOLUTION / REFRESH ====

int64_t lines_resolution(const Counter *c, const LineCfg *lc, uint8_t fmt,
                         const LineValue *v, const WatchSettings *ws, bool reveal) {
  int64_t finest = lines_finest_res(fmt);
  if (!lines_source_is_running(lc->source) || c->state != ST_RUNNING || reveal ||
      !(lc->flags & LINE_FLAG_THRESHOLDS)) {
    return finest;
  }
  return format_choose_res(v->measured, ws->thr_coarse_ms, ws->thr_fine_ms, finest);
}

int64_t lines_next_change_ms(const LineCfg *lc, const LineValue *v, int64_t res,
                             const WatchSettings *ws, bool reveal, bool live) {
  if (!live || !v->has || v->rate == 0) return -1;
  int64_t dt = format_next_change_ms(v->value, v->rate, res);
  if (!reveal && (lc->flags & LINE_FLAG_THRESHOLDS)) {
    dt = min_pos(dt, format_res_change_ms(v->measured, v->measured_rate,
                                          ws->thr_coarse_ms, ws->thr_fine_ms));
  }
  return dt;
}
