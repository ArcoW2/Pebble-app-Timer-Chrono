// win_laps.c - the lap list: number, lap time, delta vs the prior lap and
// the total at that lap. Newest first, fastest marked.
#include "win_laps.h"

#include "app.h"
#include "buttons.h"
#include "colors.h"
#include "icons.h"
#include "layout.h"
#include "touchin.h"

#define ROW_H      30
#define HEADER_H   24
#define ROW_FONT   "RESOURCE_ID_GOTHIC_18"
#define HEAD_FONT  "RESOURCE_ID_GOTHIC_18_BOLD"

enum { ACT_SCROLL_UP = 1, ACT_SCROLL_DOWN, ACT_CLOSE };

static Window  *s_window;
static Layer   *s_canvas;
static Buttons  s_buttons;
static Palette  s_palette;
static uint8_t  s_top;  // first visible row (0 = newest)

// ==== HELPERS ====

static uint8_t rows_visible(void) {
  return (uint8_t)((layout_h() - HEADER_H) / ROW_H);
}

static void fmt_ms(char *buf, size_t n, int64_t ms, bool sign) {
  const char *s = "";
  if (sign) s = ms < 0 ? "-" : "+";
  if (ms < 0) ms = -ms;
  int h = (int)(ms / 3600000);
  int m = (int)((ms / 60000) % 60);
  int sec = (int)((ms / 1000) % 60);
  if (h > 0) snprintf(buf, n, "%s%d:%02d:%02d", s, h, m, sec);
  else       snprintf(buf, n, "%s%d:%02d", s, m, sec);
}

// ==== DRAWING ====

static void draw_row(GContext *ctx, uint8_t row, int16_t y) {
  uint8_t k = (uint8_t)(g_counter.lap_stored - 1 - row);   // index in lap_ms
  uint16_t number = (uint16_t)(g_counter.lap_count - row);
  uint32_t lap = g_counter.lap_ms[k];

  int64_t total = g_counter.last_lap_end_ms;
  for (uint8_t i = (uint8_t)(k + 1); i < g_counter.lap_stored; i++) {
    total -= g_counter.lap_ms[i];
  }
  char lap_s[20], delta_s[20], total_s[20], nr_s[8];
  fmt_ms(lap_s, sizeof(lap_s), lap, false);
  fmt_ms(total_s, sizeof(total_s), total, false);
  snprintf(nr_s, sizeof(nr_s), "%u", number);
  if (k > 0) {
    fmt_ms(delta_s, sizeof(delta_s), (int64_t)lap - (int64_t)g_counter.lap_ms[k - 1],
           true);
  } else {
    snprintf(delta_s, sizeof(delta_s), "-");
  }

  GFont font = fonts_get_system_font(ROW_FONT);
  graphics_context_set_text_color(ctx, s_palette.fg);
  graphics_draw_text(ctx, nr_s, font, GRect(2, y, 26, ROW_H), GTextOverflowModeFill,
                     GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, lap_s, font, GRect(28, y, 58, ROW_H), GTextOverflowModeFill,
                     GTextAlignmentLeft, NULL);
  bool faster = (k > 0 && lap < g_counter.lap_ms[k - 1]);
  graphics_context_set_text_color(ctx, k == 0 ? s_palette.fg
                                              : (faster ? s_palette.green : s_palette.red));
  graphics_draw_text(ctx, delta_s, font, GRect(86, y, 56, ROW_H), GTextOverflowModeFill,
                     GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, s_palette.dim);
  graphics_draw_text(ctx, total_s, font, GRect(132, y, 40, ROW_H), GTextOverflowModeFill,
                     GTextAlignmentRight, NULL);
  if (lap == g_counter.lap_best_ms) {
    icon_draw(ctx, IC_BEST, GRect(layout_w() - layout_hint_w() - 12, y + 5, 11, 11),
              s_palette.accent);
  }
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  char head[40];
  uint16_t rolled = (uint16_t)(g_counter.lap_count - g_counter.lap_stored);
  if (rolled > 0) {
    snprintf(head, sizeof(head), "Laps %u (last %u)", g_counter.lap_count,
             g_counter.lap_stored);
  } else {
    snprintf(head, sizeof(head), "Laps %u", g_counter.lap_count);
  }
  graphics_context_set_text_color(ctx, s_palette.accent);
  graphics_draw_text(ctx, head, fonts_get_system_font(HEAD_FONT),
                     GRect(2, 0, layout_w() - layout_hint_w() - 4, HEADER_H), GTextOverflowModeFill,
                     GTextAlignmentLeft, NULL);

  if (g_counter.lap_stored == 0) {
    graphics_context_set_text_color(ctx, s_palette.dim);
    graphics_draw_text(ctx, "No laps yet", fonts_get_system_font(ROW_FONT),
                       GRect(2, HEADER_H + 8, layout_w() - layout_hint_w() - 4, ROW_H),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    return;
  }
  uint8_t last = (uint8_t)(s_top + rows_visible());
  if (last > g_counter.lap_stored) last = g_counter.lap_stored;
  for (uint8_t row = s_top; row < last; row++) {
    draw_row(ctx, row, (int16_t)(HEADER_H + (row - s_top) * ROW_H));
  }
}

// ==== ACTIONS ====

static void on_action(uint8_t action, int64_t press_ms, void *ctx) {
  switch (action) {
    case ACT_SCROLL_UP:
      if (s_top > 0) s_top--;
      layer_mark_dirty(s_canvas);
      break;
    case ACT_SCROLL_DOWN:
      if (g_counter.lap_stored > rows_visible() &&
          s_top < g_counter.lap_stored - rows_visible()) {
        s_top++;
        layer_mark_dirty(s_canvas);
      }
      break;
    case ACT_CLOSE:
      window_stack_pop(true);
      break;
    default:
      break;
  }
}

static void on_swipe(int8_t direction, void *ctx) {
  window_stack_pop(true);
}

// ==== WINDOW ====

static void window_appear(Window *window) {
  TouchClient client = { .on_swipe = on_swipe, .ctx = s_window };
  touchin_attach(&client);
}

static void window_disappear(Window *window) {
  touchin_detach(s_window);
}

static void window_unload(Window *window) {
  buttons_deinit(&s_buttons);
  layer_destroy(s_canvas);
  s_canvas = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

void win_laps_push(void) {
  colors_for_mode(g_counter.mode, &s_palette);
  s_top = 0;
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

  ButtonMap m;
  memset(&m, 0, sizeof(m));
  m.back_icon = IC_BACK;
  m.b[BTN_UP] = (ButtonDef){ IC_UP, IC_NONE, ACT_SCROLL_UP, 0, true };
  m.b[BTN_DOWN] = (ButtonDef){ IC_DOWN, IC_NONE, ACT_SCROLL_DOWN, 0, true };
  m.b[BTN_SELECT] = (ButtonDef){ IC_BACK, IC_NONE, ACT_CLOSE, 0, false };
  buttons_set_map(&s_buttons, &m);
  window_stack_push(s_window, true);
}
