// win_recents.c - recently used settings (mode plus value), newest first.
#include "win_recents.h"

#include "app.h"
#include "buttons.h"
#include "colors.h"
#include "icons.h"
#include "layout.h"

#define ROW_H     30
#define HEADER_H  20
#define ROW_FONT  "RESOURCE_ID_GOTHIC_18"
#define HEAD_FONT "RESOURCE_ID_GOTHIC_14_BOLD"

enum { ACT_PREV = 1, ACT_NEXT_ITEM, ACT_APPLY };

static Window  *s_window;
static Layer   *s_canvas;
static Buttons  s_buttons;
static Palette  s_palette;
static Recent   s_items[MAX_RECENTS];
static uint8_t  s_count;
static uint8_t  s_sel;

// ==== HELPERS ====

static void describe(const Recent *r, char *buf, size_t n) {
  int h = (int)(r->value_ms / 3600000);
  int m = (int)((r->value_ms / 60000) % 60);
  int s = (int)((r->value_ms / 1000) % 60);
  switch (r->mode) {
    case MODE_UP:
      if (r->value_ms == 0) snprintf(buf, n, "Count up");
      else snprintf(buf, n, "Count up +%d:%02d:%02d", h, m, s);
      break;
    case MODE_DOWN_FOR:
      if (h > 0) snprintf(buf, n, "For %d:%02d:%02d", h, m, s);
      else snprintf(buf, n, "For %d:%02d", m, s);
      break;
    default:
      if (r->day_offset > 0) {
        snprintf(buf, n, "Until %02u:%02u +%ud", r->clock_min / 60, r->clock_min % 60,
                 r->day_offset);
      } else {
        snprintf(buf, n, "Until %02u:%02u", r->clock_min / 60, r->clock_min % 60);
      }
      break;
  }
}

static uint8_t icon_for_mode(uint8_t mode) {
  if (mode == MODE_UP) return IC_MODE_UP;
  return mode == MODE_DOWN_FOR ? IC_MODE_DOWN : IC_MODE_UNTIL;
}

// ==== DRAWING ====

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_text_color(ctx, s_palette.accent);
  graphics_draw_text(ctx, "Recent", fonts_get_system_font(HEAD_FONT),
                     GRect(2, 0, layout_w() - layout_hint_w() - 4, HEADER_H), GTextOverflowModeFill,
                     GTextAlignmentLeft, NULL);
  if (s_count == 0) {
    graphics_context_set_text_color(ctx, s_palette.dim);
    graphics_draw_text(ctx, "Nothing yet", fonts_get_system_font(ROW_FONT),
                       GRect(2, HEADER_H + 10, layout_w() - layout_hint_w() - 4, ROW_H),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    return;
  }
  for (uint8_t i = 0; i < s_count; i++) {
    int16_t y = (int16_t)(HEADER_H + 4 + i * ROW_H);
    bool sel = (i == s_sel);
    if (sel) {
      graphics_context_set_fill_color(ctx, s_palette.accent);
      graphics_fill_rect(ctx, GRect(2, y, layout_w() - layout_hint_w() - 6, ROW_H - 2), 4,
                         GCornersAll);
    }
    GColor fg = sel ? s_palette.bg : s_palette.fg;
    icon_draw(ctx, icon_for_mode(s_items[i].mode), GRect(6, y + 6, 16, 16), fg);
    char text[32];
    describe(&s_items[i], text, sizeof(text));
    graphics_context_set_text_color(ctx, fg);
    graphics_draw_text(ctx, text, fonts_get_system_font(ROW_FONT),
                       GRect(28, y + 2, layout_w() - layout_hint_w() - 34, ROW_H),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  }
}

// ==== ACTIONS ====

static void on_action(uint8_t action, int64_t press_ms, void *ctx) {
  if (s_count == 0) {
    if (action == ACT_APPLY) window_stack_pop(true);
    return;
  }
  switch (action) {
    case ACT_PREV:
      s_sel = (uint8_t)((s_sel + s_count - 1) % s_count);
      layer_mark_dirty(s_canvas);
      break;
    case ACT_NEXT_ITEM:
      s_sel = (uint8_t)((s_sel + 1) % s_count);
      layer_mark_dirty(s_canvas);
      break;
    case ACT_APPLY:
      app_recents_apply(&s_items[s_sel]);
      window_stack_pop(true);
      break;
    default:
      break;
  }
}

// ==== WINDOW ====

static void window_unload(Window *window) {
  buttons_deinit(&s_buttons);
  layer_destroy(s_canvas);
  s_canvas = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

void win_recents_push(void) {
  colors_for_mode(g_counter.mode, &s_palette);
  s_count = app_recents_get(s_items);
  s_sel = 0;
  s_window = window_create();
  window_set_background_color(s_window, s_palette.bg);
  window_set_window_handlers(s_window, (WindowHandlers){ .unload = window_unload });
  Layer *root = window_get_root_layer(s_window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update_proc);
  layer_add_child(root, s_canvas);
  buttons_init(&s_buttons, s_window, on_action, NULL);
  buttons_set_colors(&s_buttons, s_palette.fg, s_palette.accent);

  ButtonMap m;
  memset(&m, 0, sizeof(m));
  m.back_icon = IC_CANCEL;
  m.b[BTN_UP] = (ButtonDef){ IC_UP, IC_NONE, ACT_PREV, 0, false };
  m.b[BTN_DOWN] = (ButtonDef){ IC_DOWN, IC_NONE, ACT_NEXT_ITEM, 0, false };
  m.b[BTN_SELECT] = (ButtonDef){ IC_CHECK, IC_NONE, ACT_APPLY, 0, false };
  buttons_set_map(&s_buttons, &m);
  window_stack_push(s_window, true);
}
