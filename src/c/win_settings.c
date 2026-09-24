// win_settings.c - watch settings: thresholds, alarms and their output,
// lap feedback. Layouts, colours and the reveal timeout live on the phone.
#include "win_settings.h"

#include "alarm_calc.h"
#include "app.h"
#include "colors.h"
#include "icons.h"
#include "layout.h"
#include "win_editor.h"

typedef enum { RK_TIME = 0, RK_ENUM } RowKind;
typedef enum { OPT_OUT = 0, OPT_REF, OPT_ZERO } OptSet;

typedef struct {
  const char *label;
  uint8_t     kind;
  uint16_t    offset;   // into AlarmCfg, or WatchSettings for the general rows
  uint8_t     opts;     // OptSet for RK_ENUM
  bool        hours;    // RK_TIME: show an hours field
} Row;

static const Row GENERAL_ROWS[] = {
  { "Coarse threshold", RK_TIME, offsetof(WatchSettings, thr_coarse_ms), 0, true },
  { "Fine threshold",   RK_TIME, offsetof(WatchSettings, thr_fine_ms),   0, true },
  { "Button feedback",  RK_ENUM, offsetof(WatchSettings, click_out), OPT_OUT, false },
};

static const Row UP_ROWS[] = {
  { "Lap alarm",        RK_ENUM, offsetof(AlarmCfg, lap_ref),      OPT_REF, false },
  { "Fixed lap time",   RK_TIME, offsetof(AlarmCfg, lap_fixed_ms), 0,       true },
  { "Lap alarm output", RK_ENUM, offsetof(AlarmCfg, lap_out),      OPT_OUT, false },
  { "Pre-warning",      RK_TIME, offsetof(AlarmCfg, prewarn_ms),   0,       false },
  { "Pre-warn output",  RK_ENUM, offsetof(AlarmCfg, prewarn_out),  OPT_OUT, false },
  { "Total alarm",      RK_TIME, offsetof(AlarmCfg, total_ms),     0,       true },
  { "Total output",     RK_ENUM, offsetof(AlarmCfg, total_out),    OPT_OUT, false },
  { "Lap feedback",     RK_ENUM, offsetof(AlarmCfg, lapfb_out),    OPT_OUT, false },
};

static const Row DOWN_ROWS[] = {
  { "Interval",         RK_TIME, offsetof(AlarmCfg, interval_ms),  0,        true },
  { "Interval output",  RK_ENUM, offsetof(AlarmCfg, interval_out), OPT_OUT,  false },
  { "Pre-end",          RK_TIME, offsetof(AlarmCfg, preend_ms),    0,        false },
  { "Pre-end output",   RK_ENUM, offsetof(AlarmCfg, preend_out),   OPT_OUT,  false },
  { "End output",       RK_ENUM, offsetof(AlarmCfg, end_out),      OPT_OUT,  false },
  { "At zero",          RK_ENUM, offsetof(AlarmCfg, at_zero),      OPT_ZERO, false },
  { "Lap feedback",     RK_ENUM, offsetof(AlarmCfg, lapfb_out),    OPT_OUT,  false },
};

static const char *SECTION_TITLE[] = { "General", "Count up", "Count down for",
                                       "Count down until" };
static const char *OUT_NAMES[]  = { "Off", "Vibrate", "Sound", "Vibrate + sound" };
static const char *REF_NAMES[]  = { "Off", "Prior lap", "Fixed", "Fastest", "Average" };
static const char *ZERO_NAMES[] = { "Overrun", "Stop" };

static Window    *s_window;
static MenuLayer *s_menu;
static Layer     *s_hints;
static Palette    s_palette;
static uint32_t  *s_edit_target;

// ==== ROW ACCESS ====

static const Row *rows_of(uint16_t section, uint16_t *count) {
  if (section == 0) {
    *count = ARRAY_LENGTH(GENERAL_ROWS);
    return GENERAL_ROWS;
  }
  if (section == 1 + MODE_UP) {
    *count = ARRAY_LENGTH(UP_ROWS);
    return UP_ROWS;
  }
  *count = ARRAY_LENGTH(DOWN_ROWS);
  return DOWN_ROWS;
}

static void *field_ptr(uint16_t section, const Row *row) {
  uint8_t *base = (section == 0) ? (uint8_t *)&g_watch
                                 : (uint8_t *)&g_watch.mode[section - 1];
  return base + row->offset;
}

static uint8_t opt_count(uint8_t opts) {
  switch (opts) {
    case OPT_REF:  return REF_COUNT;
    case OPT_ZERO: return ZERO_COUNT;
    default:       return OUT_COUNT;
  }
}

static const char *opt_name(uint8_t opts, uint8_t value) {
  switch (opts) {
    case OPT_REF:  return REF_NAMES[value < REF_COUNT ? value : 0];
    case OPT_ZERO: return ZERO_NAMES[value < ZERO_COUNT ? value : 0];
    default:       return OUT_NAMES[value < OUT_COUNT ? value : 0];
  }
}

static void value_text(uint16_t section, const Row *row, char *buf, size_t n) {
  void *p = field_ptr(section, row);
  if (row->kind == RK_ENUM) {
    snprintf(buf, n, "%s", opt_name(row->opts, *(uint8_t *)p));
    return;
  }
  uint32_t ms = *(uint32_t *)p;
  if (ms == 0) {
    snprintf(buf, n, "Off");
  } else if (ms >= 3600000) {
    snprintf(buf, n, "%u:%02u:%02u", (unsigned)(ms / 3600000),
             (unsigned)((ms / 60000) % 60), (unsigned)((ms / 1000) % 60));
  } else {
    snprintf(buf, n, "%u:%02u", (unsigned)(ms / 60000), (unsigned)((ms / 1000) % 60));
  }
}

// ==== MENU CALLBACKS ====

static uint16_t get_num_sections(MenuLayer *menu, void *ctx) {
  return 1 + MODE_COUNT;
}

static uint16_t get_num_rows(MenuLayer *menu, uint16_t section, void *ctx) {
  uint16_t count;
  rows_of(section, &count);
  return count;
}

static int16_t get_header_height(MenuLayer *menu, uint16_t section, void *ctx) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *c) {
  menu_cell_basic_header_draw(ctx, cell, SECTION_TITLE[section]);
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *index, void *c) {
  uint16_t count;
  const Row *rows = rows_of(index->section, &count);
  if (index->row >= count) return;
  char value[24];
  value_text(index->section, &rows[index->row], value, sizeof(value));
  menu_cell_basic_draw(ctx, cell, rows[index->row].label, value, NULL);
}

static void on_value_edited(bool ok, int64_t value_ms, void *ctx) {
  if (ok && s_edit_target) {
    *s_edit_target = (uint32_t)value_ms;
    app_settings_changed();
  }
  s_edit_target = NULL;
  if (s_menu) menu_layer_reload_data(s_menu);
}

static void on_select(MenuLayer *menu, MenuIndex *index, void *c) {
  uint16_t count;
  const Row *rows = rows_of(index->section, &count);
  if (index->row >= count) return;
  const Row *row = &rows[index->row];
  void *p = field_ptr(index->section, row);
  if (row->kind == RK_ENUM) {
    uint8_t *v = p;
    *v = (uint8_t)((*v + 1) % opt_count(row->opts));
    app_settings_changed();
    menu_layer_reload_data(s_menu);
    return;
  }
  s_edit_target = p;
  win_editor_push_value(row->label, *(uint32_t *)p, row->hours, on_value_edited, NULL);
}

// ==== HINTS ====

static void hints_update_proc(Layer *layer, GContext *ctx) {
  int16_t x = SCREEN_W - HINT_W + 4;
  icon_draw(ctx, IC_UP, GRect(x, 31, 14, 14), s_palette.fg);
  icon_draw(ctx, IC_EDIT, GRect(x, 107, 14, 14), s_palette.fg);
  icon_draw(ctx, IC_DOWN, GRect(x, 183, 14, 14), s_palette.fg);
  icon_draw(ctx, IC_BACK, GRect(3, 3, 12, 12), s_palette.fg);
}

// ==== WINDOW ====

static void window_unload(Window *window) {
  layer_destroy(s_hints);
  s_hints = NULL;
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

void win_settings_push(void) {
  colors_for_mode(g_counter.mode, &s_palette);
  s_window = window_create();
  window_set_background_color(s_window, s_palette.bg);
  window_set_window_handlers(s_window, (WindowHandlers){ .unload = window_unload });
  Layer *root = window_get_root_layer(s_window);

  s_menu = menu_layer_create(GRect(0, 0, SCREEN_W - HINT_W, SCREEN_H));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_sections = get_num_sections,
    .get_num_rows = get_num_rows,
    .get_header_height = get_header_height,
    .draw_header = draw_header,
    .draw_row = draw_row,
    .select_click = on_select,
  });
  menu_layer_set_normal_colors(s_menu, s_palette.bg, s_palette.fg);
  menu_layer_set_highlight_colors(s_menu, s_palette.accent, s_palette.bg);
  menu_layer_set_click_config_onto_window(s_menu, s_window);
  layer_add_child(root, menu_layer_get_layer(s_menu));

  s_hints = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_hints, hints_update_proc);
  layer_add_child(root, s_hints);
  window_stack_push(s_window, true);
}
