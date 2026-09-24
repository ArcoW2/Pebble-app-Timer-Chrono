// icons.c - icons drawn with primitives. Coordinates are percentages of
// the icon box, so every icon scales with the box it is drawn in.
#include "icons.h"

#include "draw.h"
#include "settings.h"

// Literal key: falls back to the system font if this build lacks it.
#define TINY_FONT_KEY "RESOURCE_ID_GOTHIC_09"

typedef struct {
  GContext *ctx;
  int16_t   x, y, s;   // square box
  uint8_t   w;         // stroke width
} Pen;

// ==== HELPERS ====

static GPoint pt(const Pen *p, int fx, int fy) {
  return GPoint(p->x + p->s * fx / 100, p->y + p->s * fy / 100);
}

static void ln(const Pen *p, int x0, int y0, int x1, int y1) {
  draw_line_w(p->ctx, pt(p, x0, y0), pt(p, x1, y1), p->w);
}

static void tri(const Pen *p, int ax, int ay, int bx, int by, int cx, int cy) {
  GPoint pts[3] = { pt(p, ax, ay), pt(p, bx, by), pt(p, cx, cy) };
  draw_fill_poly(p->ctx, pts, 3);
}

static void box_fill(const Pen *p, int x0, int y0, int x1, int y1) {
  GPoint a = pt(p, x0, y0), b = pt(p, x1, y1);
  graphics_fill_rect(p->ctx, GRect(a.x, a.y, b.x - a.x + 1, b.y - a.y + 1), 0,
                     GCornerNone);
}

static void circle(const Pen *p, int cx, int cy, int r) {
  graphics_context_set_stroke_width(p->ctx, p->w);
  graphics_draw_circle(p->ctx, pt(p, cx, cy), p->s * r / 100);
}

static void tiny_text(const Pen *p, const char *text, int fx, int fy) {
  GPoint a = pt(p, fx, fy);
  graphics_draw_text(p->ctx, text, fonts_get_system_font(TINY_FONT_KEY),
                     GRect(a.x, a.y - 3, p->s, 12), GTextOverflowModeFill,
                     GTextAlignmentLeft, NULL);
}

static void star(const Pen *p) {
  GPoint pts[10];
  int16_t cx = p->x + p->s / 2, cy = p->y + p->s / 2;
  for (int i = 0; i < 10; i++) {
    int32_t a = DEG_TO_TRIGANGLE(i * 36);
    int32_t r = (i % 2 == 0) ? p->s * 50 / 100 : p->s * 20 / 100;
    pts[i] = GPoint(cx + (int16_t)(r * sin_lookup(a) / TRIG_MAX_RATIO),
                    cy - (int16_t)(r * cos_lookup(a) / TRIG_MAX_RATIO));
  }
  draw_fill_poly(p->ctx, pts, 10);
}

static void flag(const Pen *p) {
  ln(p, 15, 5, 15, 95);
  box_fill(p, 18, 8, 80, 50);
}

// ==== ICONS ====

static void draw_indicator(const Pen *p, uint8_t id) {
  switch (id) {
    case IC_TOTAL:  // sigma
      ln(p, 15, 10, 85, 10); ln(p, 15, 10, 55, 50);
      ln(p, 55, 50, 15, 90); ln(p, 15, 90, 85, 90);
      break;
    case IC_REMAIN:  // hourglass
      ln(p, 15, 5, 85, 5); ln(p, 15, 95, 85, 95);
      ln(p, 20, 5, 80, 95); ln(p, 80, 5, 20, 95);
      tri(p, 30, 95, 70, 95, 50, 70);
      break;
    case IC_LAP:  // flag with running arrow
      flag(p);
      ln(p, 30, 75, 85, 75); tri(p, 75, 62, 95, 75, 75, 88);
      break;
    case IC_PRIOR:  flag(p); tiny_text(p, "-1", 45, 60); break;
    case IC_PRIOR2: flag(p); tiny_text(p, "-2", 45, 60); break;
    case IC_BEST:   star(p); break;
    case IC_AVG:    circle(p, 50, 50, 35); ln(p, 10, 90, 90, 10); break;
    case IC_DELTA:
      ln(p, 50, 8, 8, 92); ln(p, 50, 8, 92, 92); ln(p, 8, 92, 92, 92);
      break;
    case IC_UNTIL:
      circle(p, 50, 50, 44); ln(p, 50, 50, 50, 20); ln(p, 50, 50, 72, 60);
      break;
    case IC_COUNT:
      ln(p, 35, 10, 30, 90); ln(p, 70, 10, 65, 90);
      ln(p, 12, 35, 90, 35); ln(p, 10, 65, 88, 65);
      break;
    default: break;
  }
}

static void draw_action(const Pen *p, uint8_t id) {
  switch (id) {
    case IC_PLAY:   tri(p, 20, 8, 20, 92, 90, 50); break;
    case IC_PAUSE:  box_fill(p, 18, 10, 40, 90); box_fill(p, 60, 10, 82, 90); break;
    case IC_STOP:   box_fill(p, 15, 15, 85, 85); break;
    case IC_RESET:
    case IC_CANCEL: ln(p, 15, 15, 85, 85); ln(p, 85, 15, 15, 85); break;
    case IC_RESTART: {
      GPoint a = pt(p, 12, 12), b = pt(p, 88, 88);
      graphics_context_set_stroke_width(p->ctx, p->w);
      graphics_draw_arc(p->ctx, GRect(a.x, a.y, b.x - a.x, b.y - a.y),
                        GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(60),
                        DEG_TO_TRIGANGLE(350));
      tri(p, 50, 0, 50, 30, 75, 15);
      break;
    }
    case IC_EDIT:  // pencil
      ln(p, 25, 75, 80, 20); tri(p, 10, 90, 18, 64, 36, 82);
      break;
    case IC_SETTINGS:  // gear
      circle(p, 50, 50, 22);
      ln(p, 50, 5, 50, 22); ln(p, 50, 78, 50, 95);
      ln(p, 5, 50, 22, 50); ln(p, 78, 50, 95, 50);
      ln(p, 18, 18, 30, 30); ln(p, 70, 70, 82, 82);
      ln(p, 82, 18, 70, 30); ln(p, 30, 70, 18, 82);
      break;
    case IC_PRESETS:
      ln(p, 10, 20, 90, 20); ln(p, 10, 50, 90, 50); ln(p, 10, 80, 90, 80);
      break;
    case IC_NEXT:
      ln(p, 8, 50, 70, 50); tri(p, 60, 22, 95, 50, 60, 78);
      break;
    case IC_BACK:
      ln(p, 30, 50, 92, 50); tri(p, 40, 22, 5, 50, 40, 78);
      break;
    case IC_PLUS:  ln(p, 50, 10, 50, 90); ln(p, 10, 50, 90, 50); break;
    case IC_MINUS: ln(p, 10, 50, 90, 50); break;
    case IC_CHECK: ln(p, 8, 55, 38, 85); ln(p, 38, 85, 92, 18); break;
    case IC_UP:    tri(p, 50, 12, 92, 85, 8, 85); break;
    case IC_DOWN:  tri(p, 8, 15, 92, 15, 50, 88); break;
    case IC_EXIT:  // open box with arrow out
      ln(p, 55, 10, 10, 10); ln(p, 10, 10, 10, 90); ln(p, 10, 90, 55, 90);
      ln(p, 35, 50, 80, 50); tri(p, 72, 30, 98, 50, 72, 70);
      break;
    default: break;
  }
}

static void draw_mode(const Pen *p, uint8_t id) {
  bool up = (id == IC_MODE_UP);
  ln(p, 50, 12, 50, 88);
  if (up) tri(p, 50, 0, 88, 40, 12, 40);
  else    tri(p, 12, 60, 88, 60, 50, 100);
  if (id == IC_MODE_UNTIL) {
    Pen small = *p;
    small.s = p->s * 45 / 100;
    small.x = p->x + p->s - small.s;
    small.y = p->y;
    small.w = 1;
    draw_indicator(&small, IC_UNTIL);
  }
}

// ==== PUBLIC ====

void icon_draw(GContext *ctx, uint8_t id, GRect box, GColor color) {
  if (id == IC_NONE) return;
  int16_t s = box.size.w < box.size.h ? box.size.w : box.size.h;
  Pen p = {
    .ctx = ctx,
    .x = box.origin.x + (box.size.w - s) / 2,
    .y = box.origin.y + (box.size.h - s) / 2,
    .s = s,
    .w = (uint8_t)(s >= 22 ? 3 : (s >= 11 ? 2 : 1)),
  };
  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_fill_color(ctx, color);
  graphics_context_set_text_color(ctx, color);
  if (id <= IC_COUNT)          draw_indicator(&p, id);
  else if (id <= IC_CANCEL)    draw_action(&p, id);
  else                         draw_mode(&p, id);
}

uint8_t icon_for_source(uint8_t source) {
  switch (source) {
    case SRC_TOTAL:       return IC_TOTAL;
    case SRC_REMAINING:   return IC_REMAIN;
    case SRC_CUR_LAP:     return IC_LAP;
    case SRC_PRIOR:       return IC_PRIOR;
    case SRC_PRIOR2:      return IC_PRIOR2;
    case SRC_BEST:        return IC_BEST;
    case SRC_AVG:         return IC_AVG;
    case SRC_DELTA_PRIOR:
    case SRC_DELTA_BEST:
    case SRC_DELTA_AVG:   return IC_DELTA;
    case SRC_UNTIL:       return IC_UNTIL;
    case SRC_LAPCOUNT:    return IC_COUNT;
    default:              return IC_NONE;
  }
}

const char *label_for_source(uint8_t source) {
  switch (source) {
    case SRC_TOTAL:       return "TOTAL";
    case SRC_REMAINING:   return "LEFT";
    case SRC_CUR_LAP:     return "LAP";
    case SRC_PRIOR:       return "LAP-1";
    case SRC_PRIOR2:      return "LAP-2";
    case SRC_BEST:        return "BEST";
    case SRC_AVG:         return "AVG";
    case SRC_DELTA_PRIOR: return "vsLAP-1";
    case SRC_DELTA_BEST:  return "vsBEST";
    case SRC_DELTA_AVG:   return "vsAVG";
    case SRC_UNTIL:       return "UNTIL";
    case SRC_LAPCOUNT:    return "LAPS";
    default:              return "";
  }
}
