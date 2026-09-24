// app.c - global state, storage, recents.
#include "app.h"

#include "alarms.h"
#include "win_main.h"

Counter       g_counter;
PhoneSettings g_phone;
WatchSettings g_watch;

#define STORAGE_VERSION 1

enum {
  K_VERSION = 0,
  K_COUNTER,
  K_LAPS0,
  K_LAPS1,
  K_PHONE,
  K_WATCH,
  K_RECENTS,
  // K_PENDING (7) belongs to alarms.c
};

#define LAPS_PER_KEY 60

static Recent  s_recents[MAX_RECENTS];
static uint8_t s_recent_count;

// ==== TIME ====

// Monotonic: time_ms() samples seconds and milliseconds separately, so a
// composed value can step back by up to a second across a boundary. A
// displayed timer must never run backwards. A real clock change (more than
// a minute either way) is let through.
int64_t app_now_ms(void) {
  static int64_t s_last;
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  int64_t now = (int64_t)s * 1000 + ms;
  if (now < s_last && s_last - now < 60000) return s_last;
  s_last = now;
  return now;
}

const AlarmCfg *app_alarm_cfg(void) {
  uint8_t m = g_counter.mode < MODE_COUNT ? g_counter.mode : 0;
  return &g_watch.mode[m];
}

int64_t app_until_target(uint16_t clock_min, uint8_t day_offset, int64_t now) {
  time_t t = (time_t)(now / 1000);
  struct tm *lt = localtime(&t);
  int32_t sod = lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec;
  int64_t midnight = (int64_t)(t - sod) * 1000;
  int64_t target = midnight + (int64_t)clock_min * 60000;
  if (target <= now) target += 86400000LL;
  return target + (int64_t)day_offset * 86400000LL;
}

// ==== STORAGE ====

static void load_counter(void) {
  memset(&g_counter, 0, sizeof(g_counter));
  if (persist_exists(K_COUNTER)) {
    persist_read_data(K_COUNTER, &g_counter, COUNTER_HEADER_SIZE);
  } else {
    counter_setup_for(&g_counter, 5 * 60000);
    return;
  }
  // Anything out of range means the blob predates this build or is damaged.
  if (g_counter.mode >= MODE_COUNT || g_counter.state > ST_STOPPED ||
      g_counter.lap_stored > MAX_LAPS ||
      g_counter.lap_stored > g_counter.lap_count) {
    memset(&g_counter, 0, sizeof(g_counter));
    counter_setup_for(&g_counter, 5 * 60000);
    return;
  }
  if (g_counter.lap_stored > 0 && persist_exists(K_LAPS0)) {
    persist_read_data(K_LAPS0, &g_counter.lap_ms[0],
                      LAPS_PER_KEY * sizeof(uint32_t));
  }
  if (g_counter.lap_stored > LAPS_PER_KEY && persist_exists(K_LAPS1)) {
    persist_read_data(K_LAPS1, &g_counter.lap_ms[LAPS_PER_KEY],
                      (MAX_LAPS - LAPS_PER_KEY) * sizeof(uint32_t));
  }
}

void app_save_counter(void) {
  persist_write_data(K_COUNTER, &g_counter, COUNTER_HEADER_SIZE);
  if (g_counter.lap_stored > 0) {
    persist_write_data(K_LAPS0, &g_counter.lap_ms[0],
                       LAPS_PER_KEY * sizeof(uint32_t));
  }
  if (g_counter.lap_stored > LAPS_PER_KEY) {
    persist_write_data(K_LAPS1, &g_counter.lap_ms[LAPS_PER_KEY],
                       (MAX_LAPS - LAPS_PER_KEY) * sizeof(uint32_t));
  }
}

void app_save_phone(void) {
  persist_write_data(K_PHONE, &g_phone, sizeof(g_phone));
}

void app_save_watch(void) {
  persist_write_data(K_WATCH, &g_watch, sizeof(g_watch));
}

static void save_recents(void) {
  persist_write_data(K_RECENTS, s_recents, sizeof(s_recents));
  persist_write_int(K_VERSION, (STORAGE_VERSION << 8) | s_recent_count);
}

void app_load(void) {
  int32_t version = persist_exists(K_VERSION) ? persist_read_int(K_VERSION) : 0;
  bool known = ((version >> 8) == STORAGE_VERSION);

  settings_phone_defaults(&g_phone);
  settings_watch_defaults(&g_watch);
  if (known && persist_exists(K_PHONE)) {
    // A stored blob from an older struct would be read past its end, so the
    // size must match exactly, not just the version byte.
    int read = persist_read_data(K_PHONE, &g_phone, sizeof(g_phone));
    if (read != (int)sizeof(g_phone) || g_phone.version != PHONE_SETTINGS_VERSION) {
      settings_phone_defaults(&g_phone);
    }
  }
  if (known && persist_exists(K_WATCH)) {
    int read = persist_read_data(K_WATCH, &g_watch, sizeof(g_watch));
    if (read != (int)sizeof(g_watch) || g_watch.version != WATCH_SETTINGS_VERSION) {
      settings_watch_defaults(&g_watch);
    }
  }
  if (known) {
    load_counter();
    s_recent_count = (uint8_t)(version & 0xFF);
    if (s_recent_count > MAX_RECENTS) s_recent_count = 0;
    if (s_recent_count && persist_exists(K_RECENTS)) {
      persist_read_data(K_RECENTS, s_recents, sizeof(s_recents));
    }
  } else {
    memset(&g_counter, 0, sizeof(g_counter));
    counter_setup_for(&g_counter, 5 * 60000);
    s_recent_count = 0;
    persist_write_int(K_VERSION, STORAGE_VERSION << 8);
  }
}

// ==== CHANGE FLOW ====

void app_counter_changed(void) {
  app_save_counter();
  alarms_reschedule();
  win_main_refresh();
}

void app_settings_changed(void) {
  app_save_phone();
  app_save_watch();
  alarms_reschedule();
  win_main_relayout();
}

// ==== RECENTS ====

uint8_t app_recents_get(Recent *out) {
  memcpy(out, s_recents, sizeof(s_recents));
  return s_recent_count;
}

static bool same_recent(const Recent *a, const Recent *b) {
  return a->mode == b->mode && a->value_ms == b->value_ms &&
         a->clock_min == b->clock_min && a->day_offset == b->day_offset;
}

void app_recents_add_current(void) {
  Recent r = {
    .mode = g_counter.mode,
    .day_offset = g_counter.until_day_offset,
    .clock_min = g_counter.until_clock_min,
    .value_ms = (g_counter.mode == MODE_UP) ? g_counter.offset_ms : g_counter.target_ms,
  };
  uint8_t keep = 0;
  Recent merged[MAX_RECENTS];
  merged[keep++] = r;
  for (uint8_t i = 0; i < s_recent_count && keep < MAX_RECENTS; i++) {
    if (!same_recent(&s_recents[i], &r)) merged[keep++] = s_recents[i];
  }
  memcpy(s_recents, merged, sizeof(merged));
  s_recent_count = keep;
  save_recents();
}

void app_recents_apply(const Recent *r) {
  switch (r->mode) {
    case MODE_UP:       counter_setup_up(&g_counter, r->value_ms); break;
    case MODE_DOWN_FOR: counter_setup_for(&g_counter, r->value_ms); break;
    default:            counter_setup_until(&g_counter, r->clock_min, r->day_offset); break;
  }
  app_counter_changed();
  win_main_relayout();
}
