// alarm_calc.c - next alarm computation. Pure C, no pebble.h.
#include "alarm_calc.h"

// ==== HELPERS ====

static void consider(AlarmEvent *best, bool *found, int64_t at, uint8_t type) {
  // Earlier wins; on a tie the higher type (END > PREEND > INTERVAL) wins.
  if (!*found || at < best->at_ms || (at == best->at_ms && type > best->type)) {
    best->at_ms = at;
    best->type = type;
    *found = true;
  }
}

static bool wanted(const AlarmCfg *cfg, uint8_t type) {
  if (type == AL_END && cfg->at_zero == ZERO_STOP) return true;
  return alarm_output(cfg, type) != OUT_OFF;
}

// ==== PUBLIC ====

bool alarm_lap_reference(const Counter *c, const AlarmCfg *cfg, uint32_t *out) {
  switch (cfg->lap_ref) {
    case REF_PRIOR: return counter_lap_back(c, 1, out);
    case REF_FIXED: *out = cfg->lap_fixed_ms; return cfg->lap_fixed_ms > 0;
    case REF_BEST:  return counter_best(c, out);
    case REF_AVG:   return counter_average(c, out);
    default:        return false;
  }
}

uint8_t alarm_output(const AlarmCfg *cfg, uint8_t type) {
  switch (type) {
    case AL_LAP:      return cfg->lap_out;
    case AL_PREWARN:  return cfg->prewarn_out;
    case AL_TOTAL:    return cfg->total_out;
    case AL_INTERVAL: return cfg->interval_out;
    case AL_PREEND:   return cfg->preend_out;
    case AL_END:      return cfg->end_out;
    default:          return OUT_OFF;
  }
}

bool alarm_next(const Counter *c, const AlarmCfg *cfg, int64_t now, AlarmEvent *out) {
  if (c->state != ST_RUNNING) return false;
  bool found = false;

  if (c->mode == MODE_UP) {
    uint32_t ref;
    if (cfg->lap_ref != REF_OFF && alarm_lap_reference(c, cfg, &ref) && ref > 0) {
      int64_t cur = counter_current_lap_ms(c, now);
      if (cur < (int64_t)ref && wanted(cfg, AL_LAP)) {
        consider(out, &found, now + ((int64_t)ref - cur), AL_LAP);
      }
      int64_t pre = (int64_t)ref - (int64_t)cfg->prewarn_ms;
      if (cfg->prewarn_ms > 0 && pre > 0 && cur < pre && wanted(cfg, AL_PREWARN)) {
        consider(out, &found, now + (pre - cur), AL_PREWARN);
      }
    }
    if (cfg->total_ms > 0 && wanted(cfg, AL_TOTAL)) {
      int64_t active = counter_active_ms(c, now);
      if (active < (int64_t)cfg->total_ms) {
        consider(out, &found, now + ((int64_t)cfg->total_ms - active), AL_TOTAL);
      }
    }
    return found;
  }

  int64_t rem = counter_remaining_ms(c, now);
  if (rem <= 0) return false;
  if (wanted(cfg, AL_END)) consider(out, &found, now + rem, AL_END);
  if (cfg->preend_ms > 0 && rem > (int64_t)cfg->preend_ms && wanted(cfg, AL_PREEND)) {
    consider(out, &found, now + rem - (int64_t)cfg->preend_ms, AL_PREEND);
  }
  if (cfg->interval_ms > 0 && rem > (int64_t)cfg->interval_ms &&
      wanted(cfg, AL_INTERVAL)) {
    int64_t iv = (int64_t)cfg->interval_ms;
    int64_t mark = ((rem - 1) / iv) * iv;  // largest multiple below rem
    if (mark > 0) consider(out, &found, now + rem - mark, AL_INTERVAL);
  }
  return found;
}
