// win_main.c - the counter screen: the configured lines, the mode arrow,
// the hint column and the state notices.
#include "win_main.h"

#include "alarms.h"
#include "app.h"
#include "buttons.h"
#include "colors.h"
#include "icons.h"
#include "layout.h"
#include "lines.h"
#include "segment.h"
#include "touchin.h"
#include "win_editor.h"
#include "win_laps.h"
#include "win_recents.h"
#include "win_settings.h"

#define LABEL_FONT_KEY "RESOURCE_ID_GOTHIC_09"
#define STATE_FONT_KEY "RESOURCE_ID_GOTHIC_14"
#define NOTICE_MS      4000

enum {
  ACT_START = 1, ACT_PAUSE, ACT_RESUME, ACT_STOP, ACT_RESET, ACT_RESTART,
  ACT_LAP, ACT_EDIT_RUN, ACT_SETUP, ACT_PRESETS, ACT_SETTINGS, ACT_LAPS,
};

static Window   *s_window;
static Layer    *s_canvas;
static Buttons   s_buttons;
static Palette   s_palette;
static Layout    s_layout;
static uint8_t   s_fmt[MAX_LINES];
static LineText  s_text[MAX_LINES];
static AppTimer *s_refresh_timer;
static int64_t   s_reveal_until;
static char      s_notice[64];
static int64_t   s_notice_until;

static void refresh_values(void);
static void recompute_layout(void);
static bool formats_changed(int64_t now);

// ==== HELPERS ====

static int64_t min_pos(int64_t a, int64_t b) {
  if (a < 0) return b;
  if (b < 0) return a;
  return a < b ? a : b;
}

static const ModeLayout *mode_layout(void) {
  return &g_phone.mode[g_counter.mode < MODE_COUNT ? g_counter.mode : 0];
}

static const char *state_text(void) {
  switch (g_counter.state) {
    case ST_IDLE:    return "READY";
    case ST_PAUSED:  return "PAUSED";
    case ST_STOPPED: return "STOPPED";
    default:         return NULL;
  }
}

// ==== BUTTON MAP ====

static void apply_button_map(void) {
  ButtonMap m;
  memset(&m, 0, sizeof(m));
  m.back_icon = IC_EXIT;
  m.b[BTN_DOWN] = (ButtonDef){ IC_NEXT, IC_NONE, ACT_LAPS, 0, false };

  switch (g_counter.state) {
    case ST_IDLE:
      m.b[BTN_UP] = (ButtonDef){ IC_PRESETS, IC_SETTINGS, ACT_PRESETS, ACT_SETTINGS, false };
      m.b[BTN_SELECT] = (ButtonDef){ IC_PLAY, IC_EDIT, ACT_START, ACT_SETUP, false };
      break;
    case ST_RUNNING:
      if (g_counter.mode == MODE_UP) {
        m.b[BTN_UP] = (ButtonDef){ IC_LAP, IC_NONE, ACT_LAP, 0, false };
        m.b[BTN_SELECT] = (ButtonDef){ IC_PAUSE, IC_STOP, ACT_PAUSE, ACT_STOP, false };
      } else {
        m.b[BTN_UP] = (ButtonDef){ IC_LAP, IC_EDIT, ACT_LAP, ACT_EDIT_RUN, false };
        bool can_pause = counter_can_pause(&g_counter);
        m.b[BTN_SELECT] = (ButtonDef){ can_pause ? IC_PAUSE : IC_NONE, IC_STOP,
                                       can_pause ? ACT_PAUSE : 0, ACT_STOP, false };
      }
      break;
    case ST_PAUSED:
      m.b[BTN_UP] = (ButtonDef){ IC_LAP, IC_NONE, ACT_LAP, 0, false };
      m.b[BTN_SELECT] = (ButtonDef){ IC_PLAY, IC_STOP, ACT_RESUME, ACT_STOP, false };
      break;
    default:  // ST_STOPPED
      m.b[BTN_UP] = (ButtonDef){ IC_NONE, IC_SETTINGS, 0, ACT_SETTINGS, false };
      m.b[BTN_SELECT] = (ButtonDef){ IC_RESTART, IC_RESET, ACT_RESTART, ACT_RESET, false };
      break;
  }
  buttons_set_map(&s_buttons, &m);
}

// ==== DRAWING ====

static void draw_gutter(GContext *ctx, const LineGeom *g, const LineCfg *lc) {
  static const int16_t ICON_SZ[4] = { 16, 14, 12, 20 };  // L, M, S, XL
  int16_t isz = ICON_SZ[g->size < 4 ? g->size : 1];
  bool with_label = (g->size != SIZE_S);
  // Icon plus label are centred in the line box as one block.
  int16_t block_h = isz + (with_label ? 12 : 0);
  int16_t iy = g->y + (g->h - block_h) / 2;
  icon_draw(ctx, icon_for_source(lc->source), GRect(2, iy, isz, isz), s_palette.accent);
  if (!with_label) return;
  graphics_context_set_text_color(ctx, s_palette.fg);
  graphics_draw_text(ctx, label_for_source(lc->source),
                     fonts_get_system_font(LABEL_FONT_KEY),
                     GRect(0, iy + isz, g->gutter_w, 14), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
}

static void draw_mode_arrow(GContext *ctx) {
  uint8_t id = IC_MODE_UP;
  if (g_counter.mode == MODE_DOWN_FOR) id = IC_MODE_DOWN;
  if (g_counter.mode == MODE_DOWN_UNTIL) id = IC_MODE_UNTIL;
  // In the hint column, above the Down button's icon: the bottom-left
  // corner collides with the lowest line's indicator.
  icon_draw(ctx, id, GRect(SCREEN_W - HINT_W + 1, 152, 20, 20), s_palette.accent);
}

static void draw_banner(GContext *ctx, const char *text, int16_t y, GColor color) {
  graphics_context_set_fill_color(ctx, s_palette.bg);
  graphics_fill_rect(ctx, GRect(0, y, SCREEN_W - HINT_W, 18), 0, GCornerNone);
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, fonts_get_system_font(STATE_FONT_KEY),
                     GRect(2, y - 2, SCREEN_W - HINT_W - 4, 18), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  const ModeLayout *ml = mode_layout();
  SegColors colors = {
    .dim = s_palette.dim,
    .ghost = s_palette.ghost,
    .ghost_level = STYLE_GHOST_LEVEL(g_phone.style),
    .lit = s_palette.fg,
  };
  for (uint8_t i = 0; i < s_layout.n; i++) {
    const LineGeom *g = &s_layout.g[i];
    draw_gutter(ctx, g, &ml->lines[i]);
    colors.lit = colors_for_class(&s_palette, s_text[i].color);
    if (format_text_needs_sign(&s_text[i])) {
      // Overrun or saturation: narrower cells make room for the sign.
      segment_draw_text(ctx, &s_text[i], GPoint(g->text_x_sign, g->text_y), &g->dg_sign,
                        &colors);
    } else {
      LineText bare = s_text[i];   // leading blank sign cell costs nothing
      if (bare.n > 0 && bare.cells[0].kind == CELL_SIGN) {
        memmove(&bare.cells[0], &bare.cells[1], (bare.n - 1) * sizeof(Cell));
        bare.n--;
      }
      segment_draw_text(ctx, &bare, GPoint(g->text_x, g->text_y), &g->dg, &colors);
    }
  }
  draw_mode_arrow(ctx);

  const char *st = state_text();
  if (st) draw_banner(ctx, st, 2, s_palette.accent);
  if (s_notice_until > app_now_ms()) {
    draw_banner(ctx, s_notice, SCREEN_H - 20, s_palette.fg);
  }
}

// ==== VALUES / REFRESH ====

static void refresh_timer_fired(void *data) {
  s_refresh_timer = NULL;
  refresh_values();
}

static void schedule_refresh(int64_t dt) {
  if (s_refresh_timer) {
    app_timer_cancel(s_refresh_timer);
    s_refresh_timer = NULL;
  }
  if (dt < 0) return;
  if (dt < 10) dt = 10;
  if (dt > 0x7FFFFFFF) dt = 0x7FFFFFFF;
  s_refresh_timer = app_timer_register((uint32_t)dt, refresh_timer_fired, NULL);
}

static void refresh_values(void) {
  int64_t now = app_now_ms();
  if (formats_changed(now)) {   // a field appeared or disappeared
    recompute_layout();
    return;
  }
  bool reveal = now < s_reveal_until;
  bool live = counter_is_live(&g_counter);
  const ModeLayout *ml = mode_layout();
  const AlarmCfg *cfg = app_alarm_cfg();
  int64_t next = -1;
  bool changed = false;

  for (uint8_t i = 0; i < s_layout.n; i++) {
    LineValue v;
    lines_value(&g_counter, cfg, ml->lines[i].source, now, &v);
    int64_t res = lines_resolution(&g_counter, &ml->lines[i], s_fmt[i], &v, &g_watch,
                                   reveal);
    LineText t;
    format_line(&t, s_fmt[i], v.value, v.has, res, v.color);
    if (!format_text_equal(&t, &s_text[i])) {
      s_text[i] = t;
      changed = true;
    }
    next = min_pos(next, lines_next_change_ms(&ml->lines[i], &v, res, &g_watch, reveal,
                                              live));
  }
  if (reveal) next = min_pos(next, s_reveal_until - now);
  if (s_notice_until > now) next = min_pos(next, s_notice_until - now);
  if (changed || s_notice_until >= now) layer_mark_dirty(s_canvas);
  schedule_refresh(next);
}

// True when AUTO has moved to a different format, e.g. the days ran out.
static bool formats_changed(int64_t now) {
  const ModeLayout *ml = mode_layout();
  for (uint8_t i = 0; i < s_layout.n; i++) {
    if (lines_resolve_format(ml->lines[i].source, ml->lines[i].format, &g_counter,
                             now) != s_fmt[i]) {
      return true;
    }
  }
  return false;
}

static void recompute_layout(void) {
  colors_for_mode(g_counter.mode, &s_palette);
  window_set_background_color(s_window, s_palette.bg);
  buttons_set_colors(&s_buttons, s_palette.fg, s_palette.accent);

  int64_t now = app_now_ms();
  const ModeLayout *ml = mode_layout();
  for (uint8_t i = 0; i < ml->nlines; i++) {
    s_fmt[i] = lines_resolve_format(ml->lines[i].source, ml->lines[i].format, &g_counter,
                                    now);
  }
  if (!layout_compute(ml, s_fmt, &s_layout)) {
    // Should not happen (the phone config is validated), but never leave
    // the screen empty: fall back to this mode's default layout.
    ModeLayout def;
    settings_mode_layout_default((Mode)g_counter.mode, &def);
    for (uint8_t i = 0; i < def.nlines; i++) {
      s_fmt[i] = lines_resolve_format(def.lines[i].source, def.lines[i].format,
                                      &g_counter, now);
    }
    if (!layout_compute(&def, s_fmt, &s_layout)) s_layout.n = 0;
    g_phone.mode[g_counter.mode] = def;
  }
  memset(s_text, 0, sizeof(s_text));
  refresh_values();
  layer_mark_dirty(s_canvas);  // colours may change with the text unchanged
}

// ==== ACTIONS ====

static void start_counter(int64_t press_ms) {
  int64_t target = 0;
  if (g_counter.mode == MODE_DOWN_UNTIL) {
    target = app_until_target(g_counter.until_clock_min, g_counter.until_day_offset,
                              press_ms);
  }
  counter_start(&g_counter, press_ms, target);
}

static void on_action(uint8_t action, int64_t press_ms, void *ctx) {
  switch (action) {
    case ACT_START:   start_counter(press_ms); break;
    case ACT_PAUSE:   counter_pause(&g_counter, press_ms); break;
    case ACT_RESUME:  counter_resume(&g_counter, press_ms); break;
    case ACT_STOP:    counter_stop(&g_counter, press_ms); break;
    case ACT_RESET:   counter_reset(&g_counter); break;
    case ACT_RESTART: counter_reset(&g_counter); start_counter(press_ms); break;
    case ACT_LAP:
      if (counter_lap(&g_counter, press_ms)) {
        alarms_lap_feedback();
        // A lap must be visible even when no line shows lap data.
        static char lap_notice[24];
        snprintf(lap_notice, sizeof(lap_notice), "Lap %u", g_counter.lap_count);
        win_main_notice(lap_notice);
      }
      break;
    case ACT_EDIT_RUN: win_editor_push_running(); return;
    case ACT_SETUP:    win_editor_push_setup(); return;
    case ACT_PRESETS:  win_recents_push(); return;
    case ACT_SETTINGS: win_settings_push(); return;
    case ACT_LAPS:     win_laps_push(); return;
    default: return;
  }
  app_counter_changed();
  apply_button_map();
}

// ==== TOUCH ====

static void on_tap(GPoint where, void *ctx) {
  if (!counter_is_live(&g_counter)) return;  // static values are already exact
  s_reveal_until = app_now_ms() + (int64_t)g_phone.reveal_s * 1000;
  refresh_values();
  layer_mark_dirty(s_canvas);
}

static void on_swipe(int8_t direction, void *ctx) {
  win_laps_push();
}

// ==== WINDOW ====

static void window_appear(Window *window) {
  TouchClient client = { .on_tap = on_tap, .on_swipe = on_swipe, .ctx = s_window };
  touchin_attach(&client);
  recompute_layout();
  apply_button_map();
}

static void window_disappear(Window *window) {
  touchin_detach(s_window);
  if (s_refresh_timer) {
    app_timer_cancel(s_refresh_timer);
    s_refresh_timer = NULL;
  }
}

void win_main_create_and_push(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .appear = window_appear,
    .disappear = window_disappear,
  });
  Layer *root = window_get_root_layer(s_window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update_proc);
  layer_add_child(root, s_canvas);
  buttons_init(&s_buttons, s_window, on_action, NULL);  // hints on top
  window_stack_push(s_window, true);
}

void win_main_destroy(void) {
  buttons_deinit(&s_buttons);
  if (s_canvas) {
    layer_destroy(s_canvas);
    s_canvas = NULL;
  }
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}

void win_main_refresh(void) {
  if (!s_window) return;
  refresh_values();
  apply_button_map();
}

void win_main_relayout(void) {
  if (!s_window) return;
  recompute_layout();
  apply_button_map();
}

void win_main_notice(const char *text) {
  strncpy(s_notice, text, sizeof(s_notice) - 1);
  s_notice[sizeof(s_notice) - 1] = 0;
  s_notice_until = app_now_ms() + NOTICE_MS;
  if (s_canvas) {
    layer_mark_dirty(s_canvas);
    refresh_values();
  }
}
