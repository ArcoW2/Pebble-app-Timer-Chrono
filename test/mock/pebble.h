// pebble.h - MOCK header for host compile checks only.
//
// Declarations mirror the PebbleOS SDK (applib) so that `make -C test check`
// catches type and signature mistakes without the real SDK. It is NOT used
// by the Pebble build: CloudPebble compiles src/c against the real pebble.h.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define ARRAY_LENGTH(a) (sizeof(a) / sizeof((a)[0]))
#define TRIG_MAX_ANGLE 0x10000
#define TRIG_MAX_RATIO 0xffff
#define DEG_TO_TRIGANGLE(a) (((a) * TRIG_MAX_ANGLE) / 360)

typedef struct { uint8_t argb; } GColor8;
typedef GColor8 GColor;
#define GColorFromARGB8(v) ((GColor){ .argb = (v) })
#define GColorBlackARGB8 0xC0
#define GColorWhiteARGB8 0xFF
#define GColorOxfordBlueARGB8 0xC1
#define GColorDarkGrayARGB8 0xD5
#define GColorDarkGreenARGB8 0xC4
#define GColorImperialPurpleARGB8 0xD1
#define GColorYellowARGB8 0xFC
#define GColorElectricBlueARGB8 0xDF
#define GColorLightGrayARGB8 0xEA
#define GColorChromeYellowARGB8 0xF8
#define GColorOrangeARGB8 0xF4
#define GColorVividCeruleanARGB8 0xCB
#define GColorShockingPinkARGB8 0xF7
#define GColorBlack GColorFromARGB8(GColorBlackARGB8)
#define GColorWhite GColorFromARGB8(GColorWhiteARGB8)
#define GColorRed GColorFromARGB8(0xF0)
#define GColorGreen GColorFromARGB8(0xCC)
#define GColorDarkGray GColorFromARGB8(GColorDarkGrayARGB8)
#define GColorLightGray GColorFromARGB8(GColorLightGrayARGB8)
#define GColorDukeBlue GColorFromARGB8(0xC2)
#define GColorMidnightGreen GColorFromARGB8(0xC5)
#define GColorDarkCandyAppleRed GColorFromARGB8(0xE0)
#define GColorIslamicGreen GColorFromARGB8(0xC8)
#define GColorOrange GColorFromARGB8(GColorOrangeARGB8)
#define GColorRedARGB8 0xF0
#define GColorGreenARGB8 0xCC
#define GColorDarkCandyAppleRedARGB8 0xE0
#define GColorIslamicGreenARGB8 0xC8
#define GColorFollyARGB8 0xF1
#define GColorBulgarianRoseARGB8 0xD0

typedef struct { int16_t x, y; } GPoint;
typedef struct { int16_t w, h; } GSize;
typedef struct { GPoint origin; GSize size; } GRect;
#define GPoint(x, y) ((GPoint){ (int16_t)(x), (int16_t)(y) })
#define GSize(w, h) ((GSize){ (int16_t)(w), (int16_t)(h) })
#define GRect(x, y, w, h) ((GRect){ GPoint(x, y), GSize(w, h) })
#define GPointZero GPoint(0, 0)

typedef struct { uint32_t num_points; GPoint *points; int32_t rotation; GPoint offset; } GPath;
typedef struct GContext GContext;
typedef struct Layer Layer;
typedef struct Window Window;
typedef struct MenuLayer MenuLayer;
typedef void *GFont;

typedef enum { GCornerNone = 0, GCornersAll = 0x0F } GCornerMask;
typedef enum { GTextOverflowModeWordWrap, GTextOverflowModeFill } GTextOverflowMode;
typedef enum { GTextAlignmentLeft, GTextAlignmentCenter, GTextAlignmentRight } GTextAlignment;
typedef enum { GOvalScaleModeFitCircle, GOvalScaleModeFillCircle } GOvalScaleMode;
typedef void *GTextAttributes;

int32_t sin_lookup(int32_t angle);
int32_t cos_lookup(int32_t angle);
int32_t atan2_lookup(int16_t y, int16_t x);

void graphics_context_set_fill_color(GContext *ctx, GColor color);
void graphics_context_set_stroke_color(GContext *ctx, GColor color);
void graphics_context_set_text_color(GContext *ctx, GColor color);
void graphics_context_set_stroke_width(GContext *ctx, uint8_t width);
void graphics_context_set_antialiased(GContext *ctx, bool enable);
void graphics_fill_rect(GContext *ctx, GRect rect, uint16_t radius, GCornerMask mask);
void graphics_draw_round_rect(GContext *ctx, GRect rect, uint16_t radius);
void graphics_draw_line(GContext *ctx, GPoint p0, GPoint p1);
void graphics_draw_circle(GContext *ctx, GPoint p, uint16_t radius);
void graphics_draw_arc(GContext *ctx, GRect rect, GOvalScaleMode scale,
                       int32_t angle_start, int32_t angle_end);
void graphics_draw_text(GContext *ctx, const char *text, GFont font, GRect box,
                        GTextOverflowMode overflow, GTextAlignment alignment,
                        GTextAttributes *attributes);
void gpath_draw_filled(GContext *ctx, GPath *path);
bool grect_contains_point(const GRect *rect, const GPoint *point);
GFont fonts_get_system_font(const char *font_key);

typedef void (*LayerUpdateProc)(Layer *layer, GContext *ctx);
Layer *layer_create(GRect frame);
Layer *layer_create_with_data(GRect frame, size_t data_size);
void  *layer_get_data(Layer *layer);
void   layer_destroy(Layer *layer);
void   layer_set_update_proc(Layer *layer, LayerUpdateProc proc);
void   layer_add_child(Layer *parent, Layer *child);
void   layer_mark_dirty(Layer *layer);
GRect  layer_get_bounds(const Layer *layer);

typedef struct {
  void (*load)(Window *window);
  void (*appear)(Window *window);
  void (*disappear)(Window *window);
  void (*unload)(Window *window);
} WindowHandlers;

Window *window_create(void);
void    window_destroy(Window *window);
Layer  *window_get_root_layer(const Window *window);
void    window_set_background_color(Window *window, GColor color);
void    window_set_window_handlers(Window *window, WindowHandlers handlers);
void    window_stack_push(Window *window, bool animated);
void    window_stack_pop(bool animated);
void    window_stack_pop_all(bool animated);

typedef enum { BUTTON_ID_BACK = 0, BUTTON_ID_UP, BUTTON_ID_SELECT, BUTTON_ID_DOWN } ButtonId;
typedef void *ClickRecognizerRef;
typedef void (*ClickHandler)(ClickRecognizerRef recognizer, void *context);
typedef void (*ClickConfigProvider)(void *context);
ButtonId click_recognizer_get_button_id(ClickRecognizerRef recognizer);
void window_set_click_config_provider_with_context(Window *window,
                                                   ClickConfigProvider provider,
                                                   void *context);
void window_single_click_subscribe(ButtonId button_id, ClickHandler handler);
void window_raw_click_subscribe(ButtonId button_id, ClickHandler down_handler,
                                ClickHandler up_handler, void *context);

typedef struct AppTimer AppTimer;
typedef void (*AppTimerCallback)(void *data);
AppTimer *app_timer_register(uint32_t timeout_ms, AppTimerCallback callback, void *data);
void      app_timer_cancel(AppTimer *timer);

typedef enum { TouchEvent_Touchdown, TouchEvent_Liftoff, TouchEvent_PositionUpdate } TouchEventType;
typedef struct TouchEvent {
  TouchEventType type : 8;
  bool non_navigational;
  int16_t x, y;
} TouchEvent;
typedef void (*TouchServiceHandler)(const TouchEvent *event, void *context);
void touch_service_subscribe(TouchServiceHandler handler, void *context);
void touch_service_unsubscribe(void);
bool touch_service_is_enabled(void);

typedef struct { const uint32_t *durations; uint32_t num_segments; } VibePattern;
void vibes_enqueue_custom_pattern(VibePattern pattern);

typedef enum { SpeakerWaveformSine = 0 } SpeakerWaveform;
typedef struct { uint8_t midi_note, waveform; uint16_t duration_ms;
                 uint8_t velocity, reserved; } SpeakerNote;
bool speaker_play_notes(const SpeakerNote *notes, uint32_t num_notes, uint8_t volume);
bool speaker_is_muted(void);

bool    persist_exists(const uint32_t key);
int32_t persist_read_int(const uint32_t key);
int     persist_read_data(const uint32_t key, void *buffer, const size_t buffer_size);
int     persist_write_int(const uint32_t key, const int32_t value);
int     persist_write_data(const uint32_t key, const void *data, const size_t size);
int     persist_delete(const uint32_t key);

typedef int32_t WakeupId;
WakeupId wakeup_schedule(time_t timestamp, int32_t cookie, bool notify_if_missed);
void     wakeup_cancel_all(void);

typedef enum { APP_LAUNCH_SYSTEM = 0, APP_LAUNCH_USER, APP_LAUNCH_WAKEUP,
               APP_LAUNCH_QUICK_LAUNCH } AppLaunchReason;
AppLaunchReason launch_reason(void);
uint16_t time_ms(time_t *tloc, uint16_t *out_ms);
void app_event_loop(void);

typedef enum { APP_MSG_OK = 0, APP_MSG_BUSY } AppMessageResult;
typedef struct DictionaryIterator DictionaryIterator;
typedef enum { TUPLE_BYTE_ARRAY = 0, TUPLE_CSTRING, TUPLE_INT } TupleType;
typedef struct {
  uint32_t key;
  TupleType type;
  uint16_t length;
  union { const uint8_t *data; const char *cstring; int32_t int32; } const *value;
} Tuple;
Tuple *dict_find(const DictionaryIterator *iter, const uint32_t key);
uint32_t dict_write_cstring(DictionaryIterator *iter, const uint32_t key, const char *cstring);
uint32_t dict_write_uint8(DictionaryIterator *iter, const uint32_t key, const uint8_t value);
typedef void (*AppMessageInboxReceived)(DictionaryIterator *iterator, void *context);
typedef void (*AppMessageInboxDropped)(AppMessageResult reason, void *context);
void app_message_register_inbox_received(AppMessageInboxReceived handler);
void app_message_register_inbox_dropped(AppMessageInboxDropped handler);
AppMessageResult app_message_open(const uint32_t size_inbound, const uint32_t size_outbound);
AppMessageResult app_message_outbox_begin(DictionaryIterator **iterator);
AppMessageResult app_message_outbox_send(void);
#define MESSAGE_KEY_CFG 1
#define MESSAGE_KEY_CFG_STATUS 2
#define MESSAGE_KEY_CFG_REQ 3

typedef struct { uint16_t section, row; } MenuIndex;
typedef uint16_t (*MenuLayerGetNumberOfSectionsCallback)(MenuLayer *menu, void *ctx);
typedef uint16_t (*MenuLayerGetNumberOfRowsInSectionsCallback)(MenuLayer *menu, uint16_t section, void *ctx);
typedef int16_t (*MenuLayerGetHeaderHeightCallback)(MenuLayer *menu, uint16_t section, void *ctx);
typedef void (*MenuLayerDrawHeaderCallback)(GContext *ctx, const Layer *cell_layer, uint16_t section, void *c);
typedef void (*MenuLayerDrawRowCallback)(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *c);
typedef void (*MenuLayerSelectCallback)(MenuLayer *menu, MenuIndex *cell_index, void *c);
typedef struct {
  MenuLayerGetNumberOfSectionsCallback get_num_sections;
  MenuLayerGetNumberOfRowsInSectionsCallback get_num_rows;
  MenuLayerGetHeaderHeightCallback get_header_height;
  MenuLayerDrawHeaderCallback draw_header;
  MenuLayerDrawRowCallback draw_row;
  MenuLayerSelectCallback select_click;
} MenuLayerCallbacks;
#define MENU_CELL_BASIC_HEADER_HEIGHT 16
MenuLayer *menu_layer_create(GRect frame);
void menu_layer_destroy(MenuLayer *menu);
Layer *menu_layer_get_layer(const MenuLayer *menu);
void menu_layer_set_callbacks(MenuLayer *menu, void *callback_context, MenuLayerCallbacks callbacks);
void menu_layer_set_click_config_onto_window(MenuLayer *menu, Window *window);
void menu_layer_set_normal_colors(MenuLayer *menu, GColor background, GColor foreground);
void menu_layer_set_highlight_colors(MenuLayer *menu, GColor background, GColor foreground);
void menu_layer_reload_data(MenuLayer *menu);
void menu_cell_basic_draw(GContext *ctx, const Layer *cell_layer, const char *title,
                          const char *subtitle, void *icon);
void menu_cell_basic_header_draw(GContext *ctx, const Layer *cell_layer, const char *title);

#define APP_LOG_LEVEL_WARNING 1
#define APP_LOG(level, fmt, ...) ((void)0)
