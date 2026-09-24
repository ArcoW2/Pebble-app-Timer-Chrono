// alarms.c - alarm timing and output.
//
// Alarms are computed from the counter, never stored as a schedule. In the
// foreground one app_timer fires the next one; on exit the earliest alarm
// becomes a wakeup a few seconds early, so the app is running again when
// the alarm is actually due.
#include "alarms.h"

#include "alarm_calc.h"
#include "app.h"
#include "win_main.h"

#define K_PENDING       7
#define WAKE_EARLY_S    3
#define MISSED_GRACE_MS 5000
#define IDLE_EXIT_MS    20000
#define KEEP_OPEN_MS    90000

typedef struct {
  int64_t at_ms;
  uint8_t type;
} Pending;

static AppTimer *s_timer;
static AppTimer *s_exit_timer;
static AlarmEvent s_next;
static bool s_have_next;
static bool s_from_wakeup;
static int64_t s_interaction_ms;

// ==== OUTPUT ====

static void vibrate(const uint32_t *segments, uint32_t count) {
  VibePattern pattern = { .durations = segments, .num_segments = count };
  vibes_enqueue_custom_pattern(pattern);
}

static void play(const SpeakerNote *notes, uint32_t count) {
  if (speaker_is_muted()) return;
  speaker_play_notes(notes, count, 70);
}

static void output_for(uint8_t out, uint8_t type) {
  if (out == OUT_OFF) return;
  static const uint32_t SHORT2[] = { 90, 110, 90 };
  static const uint32_t SHORT1[] = { 70 };
  static const uint32_t SHORT3[] = { 90, 110, 90, 110, 90 };
  static const uint32_t LONG1[]  = { 420 };
  static const uint32_t LONG3[]  = { 450, 180, 450, 180, 450 };
  static const SpeakerNote N_LAP[]      = { { 76, 0, 140, 0, 0 }, { 0, 0, 70, 0, 0 },
                                            { 76, 0, 140, 0, 0 } };
  static const SpeakerNote N_PREWARN[]  = { { 72, 0, 120, 0, 0 } };
  static const SpeakerNote N_TOTAL[]    = { { 79, 0, 380, 0, 0 } };
  static const SpeakerNote N_INTERVAL[] = { { 72, 0, 120, 0, 0 }, { 0, 0, 70, 0, 0 },
                                            { 72, 0, 120, 0, 0 } };
  static const SpeakerNote N_PREEND[]   = { { 76, 0, 110, 0, 0 }, { 0, 0, 60, 0, 0 },
                                            { 76, 0, 110, 0, 0 }, { 0, 0, 60, 0, 0 },
                                            { 76, 0, 110, 0, 0 } };
  static const SpeakerNote N_END[]      = { { 84, 0, 350, 0, 0 }, { 0, 0, 150, 0, 0 },
                                            { 84, 0, 350, 0, 0 }, { 0, 0, 150, 0, 0 },
                                            { 84, 0, 350, 0, 0 } };
  const uint32_t *vibe = SHORT2;
  uint32_t vibe_n = ARRAY_LENGTH(SHORT2);
  const SpeakerNote *notes = N_LAP;
  uint32_t notes_n = ARRAY_LENGTH(N_LAP);

  switch (type) {
    case AL_PREWARN:
      vibe = SHORT1; vibe_n = ARRAY_LENGTH(SHORT1);
      notes = N_PREWARN; notes_n = ARRAY_LENGTH(N_PREWARN);
      break;
    case AL_TOTAL:
      vibe = LONG1; vibe_n = ARRAY_LENGTH(LONG1);
      notes = N_TOTAL; notes_n = ARRAY_LENGTH(N_TOTAL);
      break;
    case AL_INTERVAL:
      notes = N_INTERVAL; notes_n = ARRAY_LENGTH(N_INTERVAL);
      break;
    case AL_PREEND:
      vibe = SHORT3; vibe_n = ARRAY_LENGTH(SHORT3);
      notes = N_PREEND; notes_n = ARRAY_LENGTH(N_PREEND);
      break;
    case AL_END:
      vibe = LONG3; vibe_n = ARRAY_LENGTH(LONG3);
      notes = N_END; notes_n = ARRAY_LENGTH(N_END);
      break;
    default:
      break;
  }
  if (out == OUT_VIBE || out == OUT_BOTH) vibrate(vibe, vibe_n);
  if (out == OUT_SOUND || out == OUT_BOTH) play(notes, notes_n);
}

void alarms_lap_feedback(void) {
  uint8_t out = app_alarm_cfg()->lapfb_out;
  static const uint32_t TICK[] = { 45 };
  static const SpeakerNote NOTE[] = { { 88, 0, 45, 0, 0 } };
  if (out == OUT_VIBE || out == OUT_BOTH) vibrate(TICK, ARRAY_LENGTH(TICK));
  if (out == OUT_SOUND || out == OUT_BOTH) play(NOTE, ARRAY_LENGTH(NOTE));
}

void alarms_click_tick(void) {
  static const uint32_t CLICK[] = { 18 };
  static const SpeakerNote NOTE[] = { { 92, 0, 25, 0, 0 } };
  uint8_t out = g_watch.click_out;
  if (out == OUT_VIBE || out == OUT_BOTH) vibrate(CLICK, ARRAY_LENGTH(CLICK));
  if (out == OUT_SOUND || out == OUT_BOTH) play(NOTE, ARRAY_LENGTH(NOTE));
}

void alarms_clamp_tick(void) {
  static const uint32_t TICK[] = { 25 };
  vibrate(TICK, ARRAY_LENGTH(TICK));
}

static const char *type_name(uint8_t type) {
  switch (type) {
    case AL_LAP:      return "lap alarm";
    case AL_PREWARN:  return "pre-warning";
    case AL_TOTAL:    return "total alarm";
    case AL_INTERVAL: return "interval";
    case AL_PREEND:   return "pre-end";
    case AL_END:      return "end";
    default:          return "alarm";
  }
}

// ==== PENDING (for missed alarms and wakeups) ====

static void pending_clear(void) {
  if (persist_exists(K_PENDING)) persist_delete(K_PENDING);
}

static void pending_store(int64_t at_ms, uint8_t type) {
  Pending p = { .at_ms = at_ms, .type = type };
  persist_write_data(K_PENDING, &p, sizeof(p));
}

static void show_missed(const Pending *p) {
  static char text[48];
  time_t t = (time_t)(p->at_ms / 1000);
  char clock[8];
  strftime(clock, sizeof(clock), "%H:%M", localtime(&t));
  snprintf(text, sizeof(text), "Missed %s at %s", type_name(p->type), clock);
  win_main_notice(text);
  alarms_clamp_tick();
}

// ==== FIRING ====

static void stop_at_end(void) {
  if (g_counter.mode == MODE_DOWN_FOR) {
    counter_stop_at_active(&g_counter, g_counter.target_ms);
  } else if (g_counter.mode == MODE_DOWN_UNTIL) {
    counter_stop_at_active(&g_counter, g_counter.target_ms - g_counter.start_ms);
  }
}

static void schedule_idle_exit(void);

static void fire(uint8_t type) {
  const AlarmCfg *cfg = app_alarm_cfg();
  output_for(alarm_output(cfg, type), type);
  if (type == AL_END && cfg->at_zero == ZERO_STOP) {
    stop_at_end();
    app_save_counter();
  }
  win_main_refresh();
  alarms_reschedule();
  if (s_from_wakeup) schedule_idle_exit();
}

static void alarm_timer_fired(void *data) {
  s_timer = NULL;
  if (!s_have_next) return;
  uint8_t type = s_next.type;
  s_have_next = false;
  fire(type);
}

// ==== IDLE EXIT AFTER A WAKEUP ====

static void idle_exit_timer_fired(void *data) {
  s_exit_timer = NULL;
  if (!s_from_wakeup) return;
  int64_t now = app_now_ms();
  if (now - s_interaction_ms < IDLE_EXIT_MS) {
    schedule_idle_exit();
    return;
  }
  if (s_have_next && s_next.at_ms - now < KEEP_OPEN_MS) {
    schedule_idle_exit();  // another alarm is close: stay open for it
    return;
  }
  window_stack_pop_all(false);
}

static void schedule_idle_exit(void) {
  if (s_exit_timer) app_timer_cancel(s_exit_timer);
  s_exit_timer = app_timer_register(IDLE_EXIT_MS, idle_exit_timer_fired, NULL);
}

void alarms_note_interaction(void) {
  s_interaction_ms = app_now_ms();
}

// ==== SCHEDULING ====

void alarms_reschedule(void) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
  s_have_next = false;
  AlarmEvent e;
  int64_t now = app_now_ms();
  if (!alarm_next(&g_counter, app_alarm_cfg(), now, &e)) return;
  s_next = e;
  s_have_next = true;
  int64_t dt = e.at_ms - now;
  if (dt < 0) dt = 0;
  if (dt > 0x7FFFFFFF) dt = 0x7FFFFFFF;
  s_timer = app_timer_register((uint32_t)dt, alarm_timer_fired, NULL);
}

void alarms_start(bool from_wakeup) {
  s_from_wakeup = from_wakeup;
  s_interaction_ms = app_now_ms();
  wakeup_cancel_all();

  Pending p;
  if (persist_exists(K_PENDING) &&
      persist_read_data(K_PENDING, &p, sizeof(p)) == (int)sizeof(p)) {
    int64_t now = app_now_ms();
    if (p.at_ms < now - MISSED_GRACE_MS) {
      show_missed(&p);
    } else if (p.at_ms <= now) {
      fire(p.type);  // woke a moment late: fire it now
    }
  }
  pending_clear();
  alarms_reschedule();
  if (from_wakeup) schedule_idle_exit();
}

void alarms_stop_and_schedule_wakeup(void) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
  if (s_exit_timer) {
    app_timer_cancel(s_exit_timer);
    s_exit_timer = NULL;
  }
  wakeup_cancel_all();
  pending_clear();
  if (!s_have_next) return;

  pending_store(s_next.at_ms, s_next.type);
  time_t now_s = (time_t)(app_now_ms() / 1000);
  time_t wake = (time_t)(s_next.at_ms / 1000) - WAKE_EARLY_S;
  // Wakeups must be a minute apart, system wide: if the slot is taken,
  // wake earlier and let the in-app timer fire the alarm on time.
  for (int attempt = 0; attempt < 4; attempt++) {
    if (wake <= now_s + 1) return;
    if (wakeup_schedule(wake, s_next.type, true) >= 0) return;
    wake -= 65;
  }
}
