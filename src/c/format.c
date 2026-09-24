// format.c - values to LCD cells. Pure C, no pebble.h.
#include "format.h"

#include <string.h>

// Template characters:
//   O  sign/overflow cell ('>' saturated, '+' negative/overrun, else blank)
//   P  delta sign cell ('+' or '-')
//   a b  days tens/units      c e  hours tens/units
//   f g  minutes tens/units   h i  seconds tens/units   j  tenths
//   k l  count tens/units     d  letter 'd'   :  colon   .  dot
static const char *template_for(Format f) {
  switch (f) {
    case FMT_DHMS:   return "Oabdce:fg:hi";
    case FMT_HMS:    return "Oce:fg:hi";
    case FMT_MST:    return "Ofg:hi.j";
    case FMT_DELTA:  return "OPg:hi";
    case FMT_HM:     return "ce:fg";
    case FMT_COUNT2: return "kl";
    case FMT_MS:
    default:         return "Ofg:hi";
  }
}

static int64_t max_for(Format f) {
  switch (f) {
    case FMT_DHMS:   return 99LL * 86400000LL + 86399999LL;
    case FMT_HMS:    return 99LL * 3600000LL + 3599999LL;
    case FMT_DELTA:  return 9LL * 60000LL + 59999LL;
    case FMT_HM:     return 86399999LL;
    case FMT_COUNT2: return 99;
    case FMT_MST:
    case FMT_MS:
    default:         return 59LL * 60000LL + 59999LL;
  }
}

static int64_t unit_of(char t) {
  switch (t) {
    case 'a': return 864000000LL;
    case 'b': return 86400000LL;
    case 'c': return 36000000LL;
    case 'e': return 3600000LL;
    case 'f': return 600000LL;
    case 'g': return 60000LL;
    case 'h': return 10000LL;
    case 'i': return 1000LL;
    case 'j': return 100LL;
    default:  return 0;
  }
}

static bool is_digit_slot(char t) {
  return (t >= 'a' && t <= 'l') && t != 'd';
}

void format_counts(Format f, uint8_t *wide, uint8_t *narrow) {
  const char *tp = template_for(f);
  uint8_t w = 0, n = 0;
  for (; *tp; tp++) {
    if (*tp == ':' || *tp == '.') n++; else w++;
  }
  *wide = w;
  *narrow = n;
}

// ==== FORMAT ====

typedef struct {
  int64_t days, hours, mins, secs, tenths, count;
  bool ghost_days, ghost_hours;  // whole leading fields
  char ghost_tens;               // tens slot of the first lit field, or 0
} Fields;

static void split_fields(Format f, uint64_t a, Fields *fl) {
  memset(fl, 0, sizeof(*fl));
  fl->count = (int64_t)a;
  fl->days = a / 86400000ULL;
  fl->hours = (f == FMT_DHMS || f == FMT_HM) ? (a / 3600000ULL) % 24 : a / 3600000ULL;
  fl->mins = (f == FMT_MS || f == FMT_MST || f == FMT_DELTA) ? a / 60000ULL
                                                              : (a / 60000ULL) % 60;
  fl->secs = (a / 1000ULL) % 60;
  fl->tenths = (a / 100ULL) % 10;

  if (f == FMT_DHMS) {
    fl->ghost_days = (fl->days == 0);
    fl->ghost_hours = fl->ghost_days && fl->hours == 0;
    if (!fl->ghost_days)       fl->ghost_tens = (fl->days < 10) ? 'a' : 0;
    else if (!fl->ghost_hours) fl->ghost_tens = (fl->hours < 10) ? 'c' : 0;
    else                       fl->ghost_tens = (fl->mins < 10) ? 'f' : 0;
  } else if (f == FMT_HMS) {
    fl->ghost_hours = (fl->hours == 0);
    if (!fl->ghost_hours) fl->ghost_tens = (fl->hours < 10) ? 'c' : 0;
    else                  fl->ghost_tens = (fl->mins < 10) ? 'f' : 0;
  } else if (f == FMT_MS || f == FMT_MST) {
    fl->ghost_tens = (fl->mins < 10) ? 'f' : 0;
  } else if (f == FMT_COUNT2) {
    fl->ghost_tens = (fl->count < 10) ? 'k' : 0;
  }
}

static int digit_for(char t, const Fields *fl) {
  switch (t) {
    case 'a': return (int)((fl->days / 10) % 10);
    case 'b': return (int)(fl->days % 10);
    case 'c': return (int)((fl->hours / 10) % 10);
    case 'e': return (int)(fl->hours % 10);
    case 'f': return (int)((fl->mins / 10) % 10);
    case 'g': return (int)(fl->mins % 10);
    case 'h': return (int)(fl->secs / 10);
    case 'i': return (int)(fl->secs % 10);
    case 'j': return (int)fl->tenths;
    case 'k': return (int)((fl->count / 10) % 10);
    case 'l': return (int)(fl->count % 10);
    default:  return 0;
  }
}

static bool slot_in_ghost_field(char t, const Fields *fl) {
  if ((t == 'a' || t == 'b' || t == 'd') && fl->ghost_days) return true;
  if ((t == 'c' || t == 'e') && fl->ghost_hours) return true;
  return false;
}

void format_line(LineText *out, Format f, int64_t value, bool has, int64_t res_ms,
                 uint8_t color) {
  const char *tp = template_for(f);
  bool neg = value < 0;
  uint64_t a = (uint64_t)(neg ? -value : value);
  uint64_t maxv = (uint64_t)max_for(f);
  bool over = a > maxv;
  if (over) a = maxv;
  if (res_ms > 1 && f != FMT_COUNT2 && f != FMT_HM) {
    a = (a / (uint64_t)res_ms) * (uint64_t)res_ms;  // truncate, never round
  }

  Fields fl;
  split_fields(f, a, &fl);

  memset(out, 0, sizeof(*out));
  out->color = color;
  char prev = 0;
  for (uint8_t i = 0; tp[i] && i < MAX_CELLS; i++) {
    char t = tp[i];
    Cell *c = &out->cells[i];
    if (t == 'O') {
      c->kind = CELL_SIGN;
      c->ch = !has ? ' ' : over ? '>' : (neg && f != FMT_DELTA) ? '+' : ' ';
    } else if (t == 'P') {
      c->kind = CELL_SIGN;
      c->ch = !has ? ' ' : (neg ? '-' : '+');
    } else if (t == ':' || t == '.') {
      c->kind = CELL_SEP;
      c->ch = t;
      if (has && t == ':' && prev == 'e' && fl.ghost_hours) c->flags |= CF_GHOST;
    } else if (t == 'd') {
      c->kind = CELL_DIGIT;
      c->ch = 'd';
      if (has && fl.ghost_days) c->flags |= CF_GHOST;
    } else if (is_digit_slot(t)) {
      c->kind = CELL_DIGIT;
      if (!has) {
        c->ch = '-';
      } else if (unit_of(t) > 0 && unit_of(t) < res_ms) {
        c->ch = '_';
      } else {
        c->ch = (char)('0' + digit_for(t, &fl));
        if (slot_in_ghost_field(t, &fl) || (fl.ghost_tens == t && c->ch == '0')) {
          c->flags |= CF_GHOST;
        }
      }
    }
    out->n = i + 1;
    prev = t;
  }
}

bool format_text_equal(const LineText *a, const LineText *b) {
  if (a->n != b->n || a->color != b->color) return false;
  for (uint8_t i = 0; i < a->n; i++) {
    if (a->cells[i].ch != b->cells[i].ch || a->cells[i].flags != b->cells[i].flags) {
      return false;
    }
  }
  return true;
}

// ==== REFRESH TIMING ====

int64_t format_next_change_ms(int64_t v, int8_t rate, int64_t res) {
  if (rate == 0 || res <= 0) return -1;
  int64_t a = v < 0 ? -v : v;
  bool growing = (v == 0) || (v > 0 && rate > 0) || (v < 0 && rate < 0);
  int64_t k = a / res;
  if (growing) return (k + 1) * res - a;
  return a - k * res + 1;
}

int64_t format_choose_res(int64_t measured, uint32_t coarse_ms, uint32_t fine_ms,
                          int64_t finest_ms) {
  if (measured > (int64_t)coarse_ms) return 60000;
  if (measured > (int64_t)fine_ms) return 10000;
  return finest_ms;
}

static int64_t min_pos(int64_t a, int64_t b) {
  if (a < 0) return b;
  if (b < 0) return a;
  return a < b ? a : b;
}

int64_t format_res_change_ms(int64_t m, int8_t rate, uint32_t coarse_ms,
                             uint32_t fine_ms) {
  int64_t bounds[2] = { (int64_t)coarse_ms, (int64_t)fine_ms };
  int64_t best = -1;
  for (int i = 0; i < 2; i++) {
    int64_t b = bounds[i];
    if (rate > 0 && m <= b) best = min_pos(best, b - m + 1);
    if (rate < 0 && m > b)  best = min_pos(best, m - b);
  }
  return best;
}
