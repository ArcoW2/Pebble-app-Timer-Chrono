// win_editor.c - one editor for setup and for editing a running count down.
//
// Same fields, same gestures, same confirm and cancel. Up/Down or a
// circular swipe change the selected field; values clamp at their limits
// (no carry, no wrap); Select steps to the next field and then to the
// visible check; long Select confirms; Back cancels.
#include "win_editor.h"

#include "alarms.h"
#include "app.h"
#include "buttons.h"
#include "colors.h"
#include "format.h"
#include "icons.h"
#include "layout.h"
#include "segment.h"
#include "touchin.h"
#include "win_main.h"

#define TITLE_FONT_KEY "RESOURCE_ID_GOTHIC_18_BOLD"
#define INFO_FONT_KEY  "RESOURCE_ID_GOTHIC_14"
#define MAX_FIELDS     4
#define DOUBLE_TAP_WINDOW_MS 300
#define MODE_ROW_Y0    40
#define MODE_ROW_H     42
#define MODE_ROW_STEP  46
#define MODE_FONT_KEY  "RESOURCE_ID_GOTHIC_24_BOLD"

typedef enum { ED_SETUP_MODE = 0, ED_SETUP_VALUE, ED_RUNNING, ED_VALUE } EdKind;
typedef enum { FLD_DAYS = 0, FLD_HOURS, FLD_MINS, FLD_SECS, FLD_DAYOFF } FieldKind;

enum { ACT_UP = 1, ACT_DOWN, ACT_NEXT, ACT_CONFIRM, ACT_CANCEL };

static Window     *s_window;
static Layer      *s_canvas;
static Buttons     s_buttons;
static Palette     s_palette;
static AppTimer   *s_tick;
static AppTimer   *s_check_timer;

static struct {
  uint8_t     kind;
  uint8_t     mode_sel;              // ED_SETUP_MODE
  uint8_t     nfields;
  uint8_t     focus;                 // == nfields: the check
  uint8_t     fkind[MAX_FIELDS];
  int16_t     fval[MAX_FIELDS];
  int16_t     fmax[MAX_FIELDS];
  GRect       frect[MAX_FIELDS];
  GRect       check_rect;
  DigitGeom   dg;
  char        title[28];
  EditorDone  done;
  void       *done_ctx;
} s_ed;

static void editor_confirm(void);
static void editor_cancel(void);

// ==== FIELDS <-> VALUE ====

static int64_t field_unit_ms(uint8_t kind) {
  switch (kind) {
    case FLD_DAYS:  return 86400000LL;
    case FLD_HOURS: return 3600000LL;
    case FLD_MINS:  return 60000LL;
    case FLD_SECS:  return 1000LL;
    default:        return 0;  // day offset is not part of a duration
  }
}

static int64_t fields_value_ms(void) {
  int64_t v = 0;
  for (uint8_t i = 0; i < s_ed.nfields; i++) {
    v += (int64_t)s_ed.fval[i] * field_unit_ms(s_ed.fkind[i]);
  }
  return v;
}

static int16_t field_get(uint8_t kind) {
  for (uint8_t i = 0; i < s_ed.nfields; i++) {
    if (s_ed.fkind[i] == kind) return s_ed.fval[i];
  }
  return 0;
}

static uint16_t fields_clock_min(void) {
  return (uint16_t)(field_get(FLD_HOURS) * 60 + field_get(FLD_MINS));
}

static void set_fields_duration(int64_t ms) {
  for (uint8_t i = 0; i < s_ed.nfields; i++) {
    int64_t unit = field_unit_ms(s_ed.fkind[i]);
    if (unit == 0) continue;
    int64_t v = ms / unit;
    if (v > s_ed.fmax[i]) v = s_ed.fmax[i];
    s_ed.fval[i] = (int16_t)v;
    ms -= v * unit;
  }
}

static void add_field(uint8_t kind, int16_t max) {
  s_ed.fkind[s_ed.nfields] = kind;
  s_ed.fmax[s_ed.nfields] = max;
  s_ed.fval[s_ed.nfields] = 0;
  s_ed.nfields++;
}

static void build_duration_fields(bool with_days) {
  s_ed.nfields = 0;
  if (with_days) add_field(FLD_DAYS, 99);
  add_field(FLD_HOURS, with_days ? 23 : 99);
  add_field(FLD_MINS, 59);
  add_field(FLD_SECS, 59);
}

static void build_until_fields(void) {
  s_ed.nfields = 0;
  add_field(FLD_HOURS, 23);
  add_field(FLD_MINS, 59);
  add_field(FLD_DAYOFF, 99);
}

// ==== TEXT / GEOMETRY ====

// The editor renders its fields with the same segment renderer as the main
// screen, so what you set looks like what you will read.
static int8_t s_cell_field[MAX_CELLS];  // which field a cell belongs to, -1 = none

static void build_text(LineText *out) {
  memset(out, 0, sizeof(*out));
  memset(s_cell_field, -1, sizeof(s_cell_field));
  uint8_t n = 0;
  for (uint8_t i = 0; i < s_ed.nfields; i++) {
    if (s_ed.fkind[i] == FLD_DAYOFF) continue;
    if (i > 0 && s_ed.fkind[i - 1] != FLD_DAYS) {
      out->cells[n++] = (Cell){ ':', CELL_SEP, 0 };
    }
    uint8_t flags = (s_ed.focus == i) ? 0 : CF_DIM;
    s_cell_field[n] = (int8_t)i;
    out->cells[n++] = (Cell){ (char)('0' + s_ed.fval[i] / 10), CELL_DIGIT, flags };
    s_cell_field[n] = (int8_t)i;
    out->cells[n++] = (Cell){ (char)('0' + s_ed.fval[i] % 10), CELL_DIGIT, flags };
    if (s_ed.fkind[i] == FLD_DAYS) {
      s_cell_field[n] = (int8_t)i;
      out->cells[n++] = (Cell){ 'd', CELL_DIGIT, flags };
    }
  }
  out->n = n;
  out->color = COL_FG;
}

static void measure(const LineText *t, uint8_t *wide, uint8_t *narrow) {
  *wide = 0;
  *narrow = 0;
  for (uint8_t i = 0; i < t->n; i++) {
    if (t->cells[i].kind == CELL_SEP) (*narrow)++; else (*wide)++;
  }
}

// ==== DRAWING ====

static bool below_running(void) {
  if (s_ed.kind != ED_RUNNING) return false;
  int64_t now = app_now_ms();
  if (g_counter.mode == MODE_DOWN_FOR) {
    return fields_value_ms() <= counter_active_ms(&g_counter, now);
  }
  return app_until_target(fields_clock_min(), (uint8_t)field_get(FLD_DAYOFF),
                          g_counter.start_ms) <= now;
}

static void draw_info(GContext *ctx, const char *text, int16_t y, GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, fonts_get_system_font(INFO_FONT_KEY),
                     GRect(4, y, SCREEN_W - HINT_W - 8, 20), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
}

static GRect mode_row_rect(uint8_t index) {
  return GRect(4, (int16_t)(MODE_ROW_Y0 + index * MODE_ROW_STEP),
               SCREEN_W - HINT_W - 8, MODE_ROW_H);
}

static void draw_mode_options(GContext *ctx) {
  static const char *NAMES[MODE_COUNT] = { "Count up", "Down for", "Down until" };
  static const uint8_t ICONS[MODE_COUNT] = { IC_MODE_UP, IC_MODE_DOWN, IC_MODE_UNTIL };
  for (uint8_t i = 0; i < MODE_COUNT; i++) {
    GRect row = mode_row_rect(i);
    bool sel = (s_ed.mode_sel == i);
    if (sel) {
      graphics_context_set_fill_color(ctx, s_palette.accent);
      graphics_fill_rect(ctx, row, 6, GCornersAll);
    }
    GColor fg = sel ? s_palette.bg : s_palette.fg;
    icon_draw(ctx, ICONS[i], GRect(10, row.origin.y + 9, 24, 24), fg);
    graphics_context_set_text_color(ctx, fg);
    graphics_draw_text(ctx, NAMES[i], fonts_get_system_font(MODE_FONT_KEY),
                       GRect(42, row.origin.y + 4, SCREEN_W - HINT_W - 48, 32),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  }
}

static void draw_check(GContext *ctx) {
  bool focused = (s_ed.focus == s_ed.nfields);
  graphics_context_set_fill_color(ctx, focused ? s_palette.accent : s_palette.bg);
  graphics_context_set_stroke_color(ctx, s_palette.fg);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_fill_rect(ctx, s_ed.check_rect, 6, GCornersAll);
  graphics_draw_round_rect(ctx, s_ed.check_rect, 6);
  GRect inner = GRect(s_ed.check_rect.origin.x + 16, s_ed.check_rect.origin.y + 7,
                      22, 20);
  icon_draw(ctx, IC_CHECK, inner, focused ? s_palette.bg : s_palette.fg);
}

static void draw_value_editor(GContext *ctx) {
  LineText t;
  build_text(&t);
  uint8_t wide, narrow;
  measure(&t, &wide, &narrow);
  int16_t avail = SCREEN_W - HINT_W - 8;
  if (!layout_fit_digits(wide, narrow, avail, 52, 18, &s_ed.dg)) return;

  int16_t w = layout_text_width(wide, narrow, &s_ed.dg);
  int16_t x = 4 + (avail - w) / 2;
  int16_t y = 56;
  SegColors colors = {
    .lit = s_palette.fg,
    .dim = s_palette.dim,
    .ghost = s_palette.ghost,
    .ghost_level = 0,   // every digit matters while editing
  };
  // Remember where each field sits, for taps.
  for (uint8_t i = 0; i < s_ed.nfields; i++) s_ed.frect[i] = GRect(0, 0, 0, 0);
  int16_t cx = x;
  for (uint8_t i = 0; i < t.n; i++) {
    int16_t cw = (t.cells[i].kind == CELL_SEP) ? s_ed.dg.narrow_w : s_ed.dg.w;
    int8_t f = s_cell_field[i];
    if (f >= 0 && f < (int8_t)s_ed.nfields) {
      if (s_ed.frect[f].size.w == 0) s_ed.frect[f] = GRect(cx, y, cw, s_ed.dg.h);
      else s_ed.frect[f].size.w = cx + cw - s_ed.frect[f].origin.x;
    }
    cx += cw + s_ed.dg.spacing;
  }
  segment_draw_text(ctx, &t, GPoint(x, y), &s_ed.dg, &colors);
  if (s_ed.focus < s_ed.nfields && s_ed.frect[s_ed.focus].size.w > 0) {
    GRect f = s_ed.frect[s_ed.focus];   // dim vs lit alone is too subtle
    graphics_context_set_fill_color(ctx, s_palette.accent);
    graphics_fill_rect(ctx, GRect(f.origin.x, y + s_ed.dg.h + 3, f.size.w, 3), 1,
                       GCornersAll);
  }

  if (s_ed.kind == ED_SETUP_VALUE && g_counter.mode == MODE_DOWN_UNTIL) {
    static char line[40];
    int64_t target = app_until_target(fields_clock_min(),
                                      (uint8_t)field_get(FLD_DAYOFF), app_now_ms());
    time_t t_end = (time_t)(target / 1000);
    char when[24];
    strftime(when, sizeof(when), "%a %d %b", localtime(&t_end));
    snprintf(line, sizeof(line), "+%d d  %s", field_get(FLD_DAYOFF), when);
    draw_info(ctx, line, y + s_ed.dg.h + 4, s_palette.fg);
  }
  if (s_ed.kind == ED_RUNNING) {
    static char line[40];
    int64_t rem = counter_remaining_ms(&g_counter, app_now_ms());
    if (rem < 0) rem = 0;
    snprintf(line, sizeof(line), "running %d:%02d:%02d", (int)(rem / 3600000),
             (int)((rem / 60000) % 60), (int)((rem / 1000) % 60));
    draw_info(ctx, line, 30, s_palette.fg);
    if (below_running()) {
      draw_info(ctx, "below running time: OK stops", y + s_ed.dg.h + 4, s_palette.red);
    }
  }
  draw_check(ctx);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_text_color(ctx, s_palette.fg);
  graphics_draw_text(ctx, s_ed.title, fonts_get_system_font(TITLE_FONT_KEY),
                     GRect(4, 4, SCREEN_W - HINT_W - 8, 24), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
  if (s_ed.kind == ED_SETUP_MODE) {
    draw_mode_options(ctx);
  } else {
    draw_value_editor(ctx);
  }
}

// ==== BUTTONS ====

static void apply_button_map(void) {
  ButtonMap m;
  memset(&m, 0, sizeof(m));
  bool steps_back = (s_ed.kind != ED_SETUP_MODE && s_ed.focus > 0);
  m.back_icon = steps_back ? IC_BACK : IC_CANCEL;
  m.back_act = ACT_CANCEL;
  if (s_ed.kind == ED_SETUP_MODE) {
    m.b[BTN_UP] = (ButtonDef){ IC_UP, IC_NONE, ACT_UP, 0, false };
    m.b[BTN_DOWN] = (ButtonDef){ IC_DOWN, IC_NONE, ACT_DOWN, 0, false };
    m.b[BTN_SELECT] = (ButtonDef){ IC_NEXT, IC_CHECK, ACT_NEXT, ACT_CONFIRM, false };
  } else {
    bool on_check = (s_ed.focus == s_ed.nfields);
    m.b[BTN_UP] = (ButtonDef){ on_check ? IC_NONE : IC_PLUS, IC_NONE,
                               on_check ? 0 : ACT_UP, 0, !on_check };
    m.b[BTN_DOWN] = (ButtonDef){ on_check ? IC_NONE : IC_MINUS, IC_NONE,
                                 on_check ? 0 : ACT_DOWN, 0, !on_check };
    m.b[BTN_SELECT] = (ButtonDef){ on_check ? IC_CHECK : IC_NEXT, IC_CHECK,
                                   on_check ? ACT_CONFIRM : ACT_NEXT, ACT_CONFIRM,
                                   false };
  }
  buttons_set_map(&s_buttons, &m);
}

// ==== EDITING ====

static void change_field(int16_t steps) {
  if (s_ed.focus >= s_ed.nfields) return;
  int16_t v = s_ed.fval[s_ed.focus] + steps;
  int16_t max = s_ed.fmax[s_ed.focus];
  if (v < 0) v = 0;
  if (v > max) v = max;
  if (v == s_ed.fval[s_ed.focus]) {
    alarms_clamp_tick();  // clamped: no carry, no wrap
    return;
  }
  s_ed.fval[s_ed.focus] = v;
  layer_mark_dirty(s_canvas);
}

static void focus_next(void) {
  s_ed.focus = (uint8_t)((s_ed.focus + 1) % (s_ed.nfields + 1));  // wraps via the check
  apply_button_map();
  layer_mark_dirty(s_canvas);
}

static void enter_value_step(void) {
  s_ed.kind = ED_SETUP_VALUE;
  s_ed.focus = 0;
  switch (s_ed.mode_sel) {
    case MODE_UP:
      snprintf(s_ed.title, sizeof(s_ed.title), "Start offset");
      build_duration_fields(true);
      set_fields_duration(g_counter.mode == MODE_UP ? g_counter.offset_ms : 0);
      break;
    case MODE_DOWN_FOR:
      snprintf(s_ed.title, sizeof(s_ed.title), "Count down for");
      build_duration_fields(true);
      set_fields_duration(g_counter.mode == MODE_DOWN_FOR ? g_counter.target_ms
                                                          : 5 * 60000);
      break;
    default:
      snprintf(s_ed.title, sizeof(s_ed.title), "Until");
      build_until_fields();
      s_ed.fval[0] = g_counter.until_clock_min / 60;
      s_ed.fval[1] = g_counter.until_clock_min % 60;
      s_ed.fval[2] = g_counter.until_day_offset;
      break;
  }
  apply_button_map();
  layer_mark_dirty(s_canvas);
}

static void confirm_setup(void) {
  switch (s_ed.mode_sel) {
    case MODE_UP:
      counter_setup_up(&g_counter, fields_value_ms());
      break;
    case MODE_DOWN_FOR:
      counter_setup_for(&g_counter, fields_value_ms());
      break;
    default:
      counter_setup_until(&g_counter, fields_clock_min(),
                          (uint8_t)field_get(FLD_DAYOFF));
      break;
  }
  app_recents_add_current();
  app_counter_changed();
  win_main_relayout();
}

static void confirm_running(void) {
  int64_t now = app_now_ms();
  bool stop = below_running();
  if (g_counter.mode == MODE_DOWN_FOR) {
    counter_set_duration(&g_counter, fields_value_ms());
  } else {
    int64_t target = app_until_target(fields_clock_min(),
                                      (uint8_t)field_get(FLD_DAYOFF),
                                      g_counter.start_ms);
    counter_set_until(&g_counter, target, fields_clock_min(),
                      (uint8_t)field_get(FLD_DAYOFF));
  }
  if (stop) counter_stop(&g_counter, now);
  app_counter_changed();
}

static void editor_confirm(void) {
  if (s_ed.kind == ED_SETUP_MODE) {
    enter_value_step();
    return;
  }
  if (s_ed.kind == ED_SETUP_VALUE) {
    confirm_setup();
  } else if (s_ed.kind == ED_RUNNING) {
    confirm_running();
  } else if (s_ed.done) {
    s_ed.done(true, fields_value_ms(), s_ed.done_ctx);
  }
  window_stack_pop(true);
}

static void editor_back(void) {
  if (s_ed.kind != ED_SETUP_MODE && s_ed.focus > 0) {
    s_ed.focus--;                     // step back through the fields first
    apply_button_map();
    layer_mark_dirty(s_canvas);
    return;
  }
  editor_cancel();
}

static void editor_cancel(void) {
  if (s_ed.kind == ED_SETUP_VALUE) {  // back to the mode step
    s_ed.kind = ED_SETUP_MODE;
    snprintf(s_ed.title, sizeof(s_ed.title), "Mode");
    apply_button_map();
    layer_mark_dirty(s_canvas);
    return;
  }
  if (s_ed.kind == ED_VALUE && s_ed.done) s_ed.done(false, 0, s_ed.done_ctx);
  window_stack_pop(true);
}

static void on_action(uint8_t action, int64_t press_ms, void *ctx) {
  switch (action) {
    case ACT_UP:
      if (s_ed.kind == ED_SETUP_MODE) {
        s_ed.mode_sel = (uint8_t)((s_ed.mode_sel + MODE_COUNT - 1) % MODE_COUNT);
        layer_mark_dirty(s_canvas);
      } else {
        change_field(+1);
      }
      break;
    case ACT_DOWN:
      if (s_ed.kind == ED_SETUP_MODE) {
        s_ed.mode_sel = (uint8_t)((s_ed.mode_sel + 1) % MODE_COUNT);
        layer_mark_dirty(s_canvas);
      } else {
        change_field(-1);
      }
      break;
    case ACT_NEXT:
      if (s_ed.kind == ED_SETUP_MODE) enter_value_step(); else focus_next();
      break;
    case ACT_CONFIRM: editor_confirm(); break;
    case ACT_CANCEL:  editor_back(); break;
    default: break;
  }
}

// ==== TOUCH ====

static void check_timer_fired(void *data) {
  s_check_timer = NULL;
  editor_confirm();
}

static void on_tap(GPoint where, void *ctx) {
  if (s_ed.kind == ED_SETUP_MODE) {
    for (uint8_t i = 0; i < MODE_COUNT; i++) {
      GRect r = mode_row_rect(i);
      if (grect_contains_point(&r, &where)) {
        s_ed.mode_sel = i;
        layer_mark_dirty(s_canvas);
        return;
      }
    }
    return;
  }
  if (grect_contains_point(&s_ed.check_rect, &where)) {
    // A double-tap belongs to the system backlight, never to the check.
    if (s_check_timer) {
      app_timer_cancel(s_check_timer);
      s_check_timer = NULL;
      return;
    }
    s_check_timer = app_timer_register(DOUBLE_TAP_WINDOW_MS, check_timer_fired, NULL);
    return;
  }
  for (uint8_t i = 0; i < s_ed.nfields; i++) {
    GRect r = s_ed.frect[i];
    r.origin.y -= 8;
    r.size.h += 16;
    if (grect_contains_point(&r, &where)) {  // select only, never change
      s_ed.focus = i;
      apply_button_map();
      layer_mark_dirty(s_canvas);
      return;
    }
  }
}

static void on_rotate(int16_t steps, void *ctx) {
  if (s_ed.kind == ED_SETUP_MODE) return;
  change_field(steps);
}

// ==== WINDOW ====

static void tick_timer_fired(void *data) {
  s_tick = NULL;
  if (s_ed.kind != ED_RUNNING) return;
  layer_mark_dirty(s_canvas);
  s_tick = app_timer_register(1000, tick_timer_fired, NULL);
}

static void window_appear(Window *window) {
  TouchClient client = {
    .on_tap = on_tap, .on_rotate = on_rotate, .ctx = s_window,
  };
  touchin_attach(&client);
  if (s_ed.kind == ED_RUNNING && !s_tick) {
    s_tick = app_timer_register(1000, tick_timer_fired, NULL);
  }
}

static void window_disappear(Window *window) {
  touchin_detach(s_window);
  if (s_tick) {
    app_timer_cancel(s_tick);
    s_tick = NULL;
  }
  if (s_check_timer) {
    app_timer_cancel(s_check_timer);
    s_check_timer = NULL;
  }
}

static void window_unload(Window *window) {
  buttons_deinit(&s_buttons);
  layer_destroy(s_canvas);
  s_canvas = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

static void create_and_push(void) {
  colors_for_mode(g_counter.mode, &s_palette);
  s_window = window_create();
  window_set_background_color(s_window, s_palette.bg);
  window_set_window_handlers(s_window, (WindowHandlers){
    .appear = window_appear,
    .disappear = window_disappear,
    .unload = window_unload,
  });
  Layer *root = window_get_root_layer(s_window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update_proc);
  layer_add_child(root, s_canvas);
  buttons_init(&s_buttons, s_window, on_action, NULL);
  buttons_set_colors(&s_buttons, s_palette.fg, s_palette.accent);
  s_ed.check_rect = GRect((SCREEN_W - HINT_W) / 2 - 27, SCREEN_H - 46, 54, 34);
  apply_button_map();
  window_stack_push(s_window, true);
}

// ==== PUBLIC ====

void win_editor_push_setup(void) {
  memset(&s_ed, 0, sizeof(s_ed));
  s_ed.kind = ED_SETUP_MODE;
  s_ed.mode_sel = g_counter.mode;
  snprintf(s_ed.title, sizeof(s_ed.title), "Mode");
  create_and_push();
}

void win_editor_push_running(void) {
  if (g_counter.mode == MODE_UP || g_counter.state != ST_RUNNING) return;
  memset(&s_ed, 0, sizeof(s_ed));
  s_ed.kind = ED_RUNNING;
  if (g_counter.mode == MODE_DOWN_FOR) {
    snprintf(s_ed.title, sizeof(s_ed.title), "Duration");
    build_duration_fields(true);
    set_fields_duration(g_counter.target_ms);
  } else {
    snprintf(s_ed.title, sizeof(s_ed.title), "End time");
    build_until_fields();
    s_ed.fval[0] = g_counter.until_clock_min / 60;
    s_ed.fval[1] = g_counter.until_clock_min % 60;
    s_ed.fval[2] = g_counter.until_day_offset;
  }
  create_and_push();
}

void win_editor_push_value(const char *title, int64_t value_ms, bool with_hours,
                           EditorDone done, void *ctx) {
  memset(&s_ed, 0, sizeof(s_ed));
  s_ed.kind = ED_VALUE;
  s_ed.done = done;
  s_ed.done_ctx = ctx;
  strncpy(s_ed.title, title, sizeof(s_ed.title) - 1);
  s_ed.nfields = 0;
  if (with_hours) add_field(FLD_HOURS, 99);
  add_field(FLD_MINS, 59);
  add_field(FLD_SECS, 59);
  set_fields_duration(value_ms);
  create_and_push();
}
