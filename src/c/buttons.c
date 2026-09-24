// buttons.c - button engine and on-screen hints.
#include "buttons.h"

#include "alarms.h"
#include "app.h"
#include "draw.h"
#include "icons.h"
#include "layout.h"

#define ICON 14
static const int16_t ZONE_CY[BTN_COUNT] = { 38, 114, 190 };

// ==== HELPERS ====

static int8_t index_for(ButtonId id) {
  switch (id) {
    case BUTTON_ID_UP:     return BTN_UP;
    case BUTTON_ID_SELECT: return BTN_SELECT;
    case BUTTON_ID_DOWN:   return BTN_DOWN;
    default:               return -1;
  }
}

static void cancel_timer(Buttons *b) {
  if (b->timer) {
    app_timer_cancel(b->timer);
    b->timer = NULL;
  }
}

static void mark_hints_dirty(Buttons *b) {
  if (b->hints) layer_mark_dirty(b->hints);
}

// The action may change the map, push or pop windows: callers must not
// touch button state after calling this.
static void fire(Buttons *b, uint8_t action, int64_t press_ms) {
  if (action && b->fn) b->fn(action, press_ms, b->ctx);
}

// ==== HINT COLUMN ====

static void draw_hold_arrow(GContext *ctx, int16_t x, int16_t y, GColor c,
                            GColor accent, int16_t fill_px) {
  graphics_context_set_stroke_color(ctx, c);
  graphics_context_set_fill_color(ctx, c);
  draw_line_w(ctx, GPoint(x, y), GPoint(x + ICON - 4, y), 2);
  GPoint head[3] = { { x + ICON - 5, y - 3 }, { x + ICON, y }, { x + ICON - 5, y + 3 } };
  draw_fill_poly(ctx, head, 3);
  if (fill_px > 0) {
    graphics_context_set_fill_color(ctx, accent);
    graphics_fill_rect(ctx, GRect(x, y - 2, fill_px, 5), 0, GCornerNone);
  }
}

static void hints_update_proc(Layer *layer, GContext *ctx) {
  Buttons *b = *(Buttons **)layer_get_data(layer);
  int16_t x = SCREEN_W - HINT_W + (HINT_W - ICON) / 2;
  for (int i = 0; i < BTN_COUNT; i++) {
    const ButtonDef *d = &b->map.b[i];
    int16_t cy = ZONE_CY[i];
    bool pressed = (b->held == i);
    GColor sc = pressed ? b->accent : b->fg;
    if (d->short_icon && d->long_icon) {
      icon_draw(ctx, d->short_icon, GRect(x, cy - 22, ICON, ICON), sc);
      icon_draw(ctx, d->long_icon, GRect(x, cy - 3, ICON, ICON), b->fg);
    } else if (d->short_icon) {
      icon_draw(ctx, d->short_icon, GRect(x, cy - ICON / 2, ICON, ICON), sc);
      continue;
    } else if (d->long_icon) {
      icon_draw(ctx, d->long_icon, GRect(x, cy - 12, ICON, ICON), b->fg);
    } else {
      continue;
    }
    int16_t ay = d->short_icon ? cy + 15 : cy + 6;
    int16_t fill = 0;
    if (pressed) {
      int64_t held = app_now_ms() - b->press_ms;
      fill = (int16_t)((held >= LONG_PRESS_MS ? LONG_PRESS_MS : held) * ICON / LONG_PRESS_MS);
    }
    draw_hold_arrow(ctx, x, ay, b->fg, b->accent, fill);
  }
  if (b->map.back_icon) {
    icon_draw(ctx, b->map.back_icon, GRect(3, 3, ICON - 2, ICON - 2), b->fg);
  }
}

// ==== TIMERS ====

static void repeat_timer_fired(void *data) {
  Buttons *b = data;
  b->timer = NULL;
  if (b->held < 0) return;
  b->repeats++;
  uint32_t delay = b->repeats < 4 ? 200 : (b->repeats < 12 ? 100 : 50);
  b->timer = app_timer_register(delay, repeat_timer_fired, b);
  fire(b, b->map.b[b->held].short_act, app_now_ms());
}

static void progress_timer_fired(void *data) {
  Buttons *b = data;
  b->timer = NULL;
  if (b->held < 0) return;
  mark_hints_dirty(b);
  if (app_now_ms() - b->press_ms <= LONG_PRESS_MS) {
    b->timer = app_timer_register(40, progress_timer_fired, b);
  }
}

// ==== CLICK HANDLERS ====

static void raw_down_handler(ClickRecognizerRef rec, void *context) {
  Buttons *b = context;
  int8_t i = index_for(click_recognizer_get_button_id(rec));
  if (i < 0 || b->held >= 0) return;
  alarms_note_interaction();
  const ButtonDef d = b->map.b[i];
  int64_t now = app_now_ms();
  if (d.repeat) {
    b->held = i;
    b->press_ms = now;
    b->repeats = 0;
    b->timer = app_timer_register(400, repeat_timer_fired, b);
    mark_hints_dirty(b);
    fire(b, d.short_act, now);
  } else if (!d.long_act) {
    fire(b, d.short_act, now);  // no long action: act on press
  } else {
    b->held = i;
    b->press_ms = now;
    b->timer = app_timer_register(40, progress_timer_fired, b);
    mark_hints_dirty(b);
  }
}

static void raw_up_handler(ClickRecognizerRef rec, void *context) {
  Buttons *b = context;
  int8_t i = index_for(click_recognizer_get_button_id(rec));
  if (i < 0 || b->held != i) return;
  const ButtonDef d = b->map.b[i];
  int64_t press = b->press_ms;
  int64_t held_ms = app_now_ms() - press;
  cancel_timer(b);
  b->held = -1;
  mark_hints_dirty(b);
  if (!d.repeat && d.long_act) {
    fire(b, held_ms >= LONG_PRESS_MS ? d.long_act : d.short_act, press);
  }
}

static void back_click_handler(ClickRecognizerRef rec, void *context) {
  Buttons *b = context;
  alarms_note_interaction();
  if (b->map.back_act) {
    fire(b, b->map.back_act, app_now_ms());
  } else {
    window_stack_pop(true);
  }
}

static void click_config_provider(void *context) {
  window_raw_click_subscribe(BUTTON_ID_UP, raw_down_handler, raw_up_handler, context);
  window_raw_click_subscribe(BUTTON_ID_SELECT, raw_down_handler, raw_up_handler, context);
  window_raw_click_subscribe(BUTTON_ID_DOWN, raw_down_handler, raw_up_handler, context);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
}

// ==== PUBLIC ====

void buttons_init(Buttons *b, Window *window, ButtonActionFn fn, void *ctx) {
  memset(b, 0, sizeof(*b));
  b->window = window;
  b->fn = fn;
  b->ctx = ctx;
  b->held = -1;
  b->fg = GColorWhite;
  b->accent = GColorOrange;
  Layer *root = window_get_root_layer(window);
  b->hints = layer_create_with_data(layer_get_bounds(root), sizeof(Buttons *));
  *(Buttons **)layer_get_data(b->hints) = b;
  layer_set_update_proc(b->hints, hints_update_proc);
  layer_add_child(root, b->hints);
  window_set_click_config_provider_with_context(window, click_config_provider, b);
}

void buttons_set_map(Buttons *b, const ButtonMap *map) {
  b->map = *map;
  mark_hints_dirty(b);
}

void buttons_set_colors(Buttons *b, GColor fg, GColor accent) {
  b->fg = fg;
  b->accent = accent;
  mark_hints_dirty(b);
}

void buttons_deinit(Buttons *b) {
  cancel_timer(b);
  b->held = -1;
  if (b->hints) {
    layer_destroy(b->hints);
    b->hints = NULL;
  }
}
