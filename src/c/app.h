// app.h - global state, storage and the one place where a change to the
// counter is saved, re-alarmed and redrawn.
#pragma once

#include <pebble.h>

#include "model.h"
#include "settings.h"

extern Counter       g_counter;
extern PhoneSettings g_phone;
extern WatchSettings g_watch;

int64_t         app_now_ms(void);
const AlarmCfg *app_alarm_cfg(void);

// Next occurrence of a wall-clock time, plus whole days.
int64_t app_until_target(uint16_t clock_min, uint8_t day_offset, int64_t now);

void app_load(void);
void app_save_counter(void);
void app_save_phone(void);
void app_save_watch(void);

// Flow: everything that changes the counter or the settings goes through
// these two, so saving, alarms and the screen never drift apart.
void app_counter_changed(void);
void app_settings_changed(void);

typedef struct {
  uint8_t  mode;
  uint8_t  day_offset;
  uint16_t clock_min;
  int64_t  value_ms;
} Recent;

#define MAX_RECENTS 5

uint8_t app_recents_get(Recent *out);      // newest first
void    app_recents_add_current(void);
void    app_recents_apply(const Recent *r);
