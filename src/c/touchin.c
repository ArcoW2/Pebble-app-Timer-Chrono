// touchin.c - touch recognizer on top of touch_service.
#include "touchin.h"

#include "alarms.h"
#include "app.h"
#include "layout.h"

#ifdef PBL_TOUCH

#define TAP_MAX_MOVE     12     // px
#define TAP_MAX_MS       350
#define GRACE_MS         400    // after attach
#define SWIPE_MIN_DX     40     // px
#define ROTATE_MIN_R     24     // px from the centre
#define ROTATE_STEP      (TRIG_MAX_ANGLE * 20 / 360)   // one step per 20 degrees

typedef struct {
  TouchClient client;
  bool        attached;
  int64_t     attach_ms;
  // current gesture
  bool        active;
  bool        ignore;       // non-navigational contact
  bool        rotated;
  int64_t     down_ms;
  GPoint      down;
  GPoint      last;
  int16_t     max_move;
  int32_t     last_angle;
  int64_t     last_angle_ms;
  int32_t     accum;
} TouchState;

static TouchState s;

// ==== HELPERS ====

static int16_t abs16(int16_t v) { return v < 0 ? -v : v; }

static int32_t angle_of(GPoint p) {
  return atan2_lookup((int16_t)(p.y - layout_h() / 2), (int16_t)(p.x - layout_w() / 2));
}

static bool far_from_centre(GPoint p) {
  int32_t dx = p.x - layout_w() / 2, dy = p.y - layout_h() / 2;
  return dx * dx + dy * dy >= ROTATE_MIN_R * ROTATE_MIN_R;
}

static int32_t wrap_angle(int32_t d) {
  if (d > TRIG_MAX_ANGLE / 2) d -= TRIG_MAX_ANGLE;
  if (d < -TRIG_MAX_ANGLE / 2) d += TRIG_MAX_ANGLE;
  return d;
}

// Speed-based step size: degrees per second of the finger.
static int16_t step_multiplier(int32_t delta, int64_t dt_ms) {
  if (dt_ms <= 0) dt_ms = 1;
  int32_t deg_per_s = (int32_t)((int64_t)(delta < 0 ? -delta : delta) * 360 * 1000 /
                                TRIG_MAX_ANGLE / dt_ms);
  if (deg_per_s > 720) return 10;
  if (deg_per_s > 360) return 5;
  return 1;
}

// ==== GESTURE STAGES ====

static void gesture_begin(const TouchEvent *e, int64_t now) {
  s.active = true;
  s.ignore = e->non_navigational;
  s.rotated = false;
  s.down_ms = now;
  s.down = GPoint(e->x, e->y);
  s.last = s.down;
  s.max_move = 0;
  s.accum = 0;
  s.last_angle = angle_of(s.down);
  s.last_angle_ms = now;
}

static void gesture_move(const TouchEvent *e, int64_t now) {
  GPoint p = GPoint(e->x, e->y);
  int16_t move = abs16(p.x - s.down.x) + abs16(p.y - s.down.y);
  if (move > s.max_move) s.max_move = move;
  s.last = p;
  if (!s.client.on_rotate || !far_from_centre(p)) return;

  int32_t a = angle_of(p);
  int32_t d = wrap_angle(a - s.last_angle);
  int16_t mult = step_multiplier(d, now - s.last_angle_ms);
  s.last_angle = a;
  s.last_angle_ms = now;
  s.accum += d;
  int16_t steps = 0;
  while (s.accum >= ROTATE_STEP)  { steps++; s.accum -= ROTATE_STEP; }
  while (s.accum <= -ROTATE_STEP) { steps--; s.accum += ROTATE_STEP; }
  if (steps != 0 && s.max_move > TAP_MAX_MOVE) {
    s.rotated = true;
    alarms_note_interaction();
    s.client.on_rotate(steps * mult, s.client.ctx);
  }
}

static void gesture_end(int64_t now) {
  s.active = false;
  if (s.rotated) return;
  if (now - s.attach_ms < GRACE_MS) return;
  int16_t dx = s.last.x - s.down.x, dy = s.last.y - s.down.y;
  if (s.max_move <= TAP_MAX_MOVE && now - s.down_ms <= TAP_MAX_MS) {
    alarms_note_interaction();
    if (s.client.on_tap) s.client.on_tap(s.down, s.client.ctx);
  } else if (abs16(dx) >= SWIPE_MIN_DX && abs16(dx) > 2 * abs16(dy)) {
    alarms_note_interaction();
    if (s.client.on_swipe) s.client.on_swipe(dx < 0 ? SWIPE_LEFT : SWIPE_RIGHT, s.client.ctx);
  }
}

static void touch_handler(const TouchEvent *e, void *context) {
  if (!s.attached) return;
  int64_t now = app_now_ms();
  switch (e->type) {
    case TouchEvent_Touchdown:
      gesture_begin(e, now);
      break;
    case TouchEvent_PositionUpdate:
      if (s.active && !s.ignore) gesture_move(e, now);
      break;
    case TouchEvent_Liftoff:
      if (s.active && !s.ignore) gesture_end(now);
      s.active = false;
      break;
    default:
      break;
  }
}

#endif  // PBL_TOUCH

// ==== PUBLIC ====

// Touch exists on emery only; elsewhere every function is on the buttons.
void touchin_attach(const TouchClient *client) {
#ifdef PBL_TOUCH
  s.client = *client;
  s.attach_ms = app_now_ms();
  s.active = false;
  if (!s.attached) touch_service_subscribe(touch_handler, NULL);
  s.attached = true;
#else
  (void)client;
#endif
}

void touchin_detach(void *ctx) {
#ifdef PBL_TOUCH
  if (!s.attached || s.client.ctx != ctx) return;
  touch_service_unsubscribe();
  s.attached = false;
  s.active = false;
#else
  (void)ctx;
#endif
}
