// test_main.c - host unit tests for the pure modules (model, format,
// lines, alarm_calc, layout, settings). Build and run: `make -C test`.
#include <stdio.h>
#include <string.h>

#include "../src/c/alarm_calc.h"
#include "../src/c/format.h"
#include "../src/c/layout.h"
#include "../src/c/lines.h"
#include "../src/c/model.h"
#include "../src/c/settings.h"

static int s_fail = 0, s_pass = 0;

#define CHECK(cond) do { \
  if (cond) { s_pass++; } else { s_fail++; \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

// Renders a LineText as a string; ghost cells as '8' in lower-case 'g'.
static const char *txt(const LineText *t) {
  static char buf[64];
  int k = 0;
  for (int i = 0; i < t->n; i++) {
    buf[k++] = (t->cells[i].flags & CF_GHOST) ? '~' : t->cells[i].ch;
  }
  buf[k] = 0;
  return buf;
}

static void test_format(void) {
  LineText t;
  format_line(&t, FMT_MS, 4 * 60000 + 37500, true, 1000, COL_FG);
  CHECK(strcmp(txt(&t), " ~4:37") == 0);  // sign blank, tens ghosted

  format_line(&t, FMT_MS, 4 * 60000 + 37500, true, 10000, COL_FG);
  CHECK(strcmp(txt(&t), " ~4:3_") == 0);  // truncated, placeholder

  format_line(&t, FMT_MS, 4 * 60000 + 37500, true, 60000, COL_FG);
  CHECK(strcmp(txt(&t), " ~4:__") == 0);

  format_line(&t, FMT_HMS, 12 * 60000 + 5000, true, 1000, COL_FG);
  CHECK(strcmp(txt(&t), " ~~~12:05") == 0);  // hours field + colon ghosted

  format_line(&t, FMT_DHMS, 86400000LL + 3600000LL * 23 + 45 * 60000, true, 1000, COL_FG);
  CHECK(strcmp(txt(&t), " ~1d23:45:00") == 0);

  format_line(&t, FMT_DHMS, 4 * 60000 + 12000, true, 1000, COL_FG);
  CHECK(strcmp(txt(&t), " ~~~~~~~4:12") == 0);

  format_line(&t, FMT_MS, 75 * 60000LL, true, 1000, COL_FG);
  CHECK(strcmp(txt(&t), ">59:59") == 0);  // saturated

  format_line(&t, FMT_MS, -23000, true, 1000, COL_RED);
  CHECK(strcmp(txt(&t), "+~0:23") == 0);  // overrun

  format_line(&t, FMT_DELTA, -12000, true, 1000, COL_GREEN);
  CHECK(strcmp(txt(&t), " -0:12") == 0);
  format_line(&t, FMT_DELTA, 12 * 60000LL, true, 1000, COL_RED);
  CHECK(strcmp(txt(&t), ">+9:59") == 0);

  format_line(&t, FMT_MST, 61234, true, 100, COL_FG);
  CHECK(strcmp(txt(&t), " ~1:01.2") == 0);

  format_line(&t, FMT_MS, 0, false, 1000, COL_FG);
  CHECK(strcmp(txt(&t), " --:--") == 0);

  format_line(&t, FMT_HM, (17 * 60 + 30) * 60000LL, true, 1000, COL_FG);
  CHECK(strcmp(txt(&t), "17:30") == 0);

  format_line(&t, FMT_COUNT2, 7, true, 1000, COL_FG);
  CHECK(strcmp(txt(&t), "~7") == 0);
}

static void test_next_change(void) {
  CHECK(format_next_change_ms(4500, +1, 1000) == 500);
  CHECK(format_next_change_ms(30000, -1, 10000) == 1);     // 3_ -> 2_ right away
  CHECK(format_next_change_ms(29999, -1, 10000) == 10000);
  CHECK(format_next_change_ms(-500, -1, 1000) == 500);     // overrun grows
  CHECK(format_next_change_ms(-1500, +1, 1000) == 501);    // delta shrinking
  CHECK(format_next_change_ms(1000, 0, 1000) < 0);

  CHECK(format_choose_res(11 * 60000, 600000, 60000, 1000) == 60000);
  CHECK(format_choose_res(5 * 60000, 600000, 60000, 1000) == 10000);
  CHECK(format_choose_res(30000, 600000, 60000, 1000) == 1000);
  CHECK(format_res_change_ms(61000, -1, 600000, 60000) == 1000);
  CHECK(format_res_change_ms(59000, +1, 600000, 60000) == 1001);
}

static void test_model(void) {
  Counter c;
  memset(&c, 0, sizeof(c));
  counter_setup_up(&c, 0);
  counter_start(&c, 1000, 0);
  CHECK(counter_active_ms(&c, 6000) == 5000);
  counter_pause(&c, 6000);
  CHECK(counter_active_ms(&c, 60000) == 5000);  // paused: frozen
  counter_resume(&c, 10000);
  CHECK(counter_lap(&c, 13000));                // lap = 5 s + 3 s, pause excluded
  uint32_t v;
  CHECK(counter_lap_back(&c, 1, &v) && v == 8000);
  counter_pause(&c, 14000);
  counter_resume(&c, 20000);
  CHECK(counter_lap(&c, 24000));                // 1 s + 4 s
  CHECK(counter_lap_back(&c, 1, &v) && v == 5000);
  CHECK(counter_best(&c, &v) && v == 5000);
  CHECK(counter_average(&c, &v) && v == 6500);
  CHECK(counter_lap(&c, 24000) == false);       // zero-length lap ignored

  counter_setup_for(&c, 10 * 60000);
  counter_start(&c, 0, 0);
  CHECK(counter_remaining_ms(&c, 60000) == 9 * 60000);
  counter_stop(&c, 60000);
  CHECK(c.state == ST_STOPPED && counter_remaining_ms(&c, 999999) == 9 * 60000);

  counter_setup_until(&c, 17 * 60 + 30, 0);
  CHECK(!counter_can_pause(&c));
  counter_start(&c, 1000, 1000 + 3600000);
  CHECK(!counter_can_pause(&c));
  CHECK(counter_remaining_ms(&c, 1000 + 600000) == 3000000);

  // Lap rolling keeps aggregates.
  counter_setup_up(&c, 0);
  counter_start(&c, 0, 0);
  for (int i = 1; i <= MAX_LAPS + 5; i++) counter_lap(&c, (int64_t)i * 1000);
  CHECK(c.lap_stored == MAX_LAPS && c.lap_count == MAX_LAPS + 5);
  CHECK(counter_average(&c, &v) && v == 1000);
}

static void test_alarms(void) {
  Counter c;
  AlarmCfg a;
  WatchSettings ws;
  settings_watch_defaults(&ws);
  a = ws.mode[MODE_DOWN_FOR];
  a.interval_ms = 5 * 60000;
  a.preend_ms = 60000;
  memset(&c, 0, sizeof(c));
  counter_setup_for(&c, 17 * 60000);
  counter_start(&c, 0, 0);
  AlarmEvent e;
  CHECK(alarm_next(&c, &a, 0, &e) && e.type == AL_INTERVAL && e.at_ms == 2 * 60000);
  CHECK(alarm_next(&c, &a, 2 * 60000, &e) && e.at_ms == 7 * 60000);
  CHECK(alarm_next(&c, &a, 15 * 60000, &e) && e.type == AL_PREEND && e.at_ms == 16 * 60000);
  CHECK(alarm_next(&c, &a, 16 * 60000, &e) && e.type == AL_END && e.at_ms == 17 * 60000);
  CHECK(!alarm_next(&c, &a, 17 * 60000, &e));

  a = ws.mode[MODE_UP];
  a.lap_ref = REF_FIXED;
  a.lap_fixed_ms = 60000;
  a.prewarn_ms = 10000;
  counter_setup_up(&c, 0);
  counter_start(&c, 0, 0);
  CHECK(alarm_next(&c, &a, 0, &e) && e.type == AL_PREWARN && e.at_ms == 50000);
  CHECK(alarm_next(&c, &a, 50000, &e) && e.type == AL_LAP && e.at_ms == 60000);
  counter_lap(&c, 70000);
  CHECK(alarm_next(&c, &a, 70000, &e) && e.at_ms == 120000);
}

static void test_lines_and_layout(void) {
  PhoneSettings p;
  settings_phone_defaults(&p);
  for (int m = 0; m < MODE_COUNT; m++) {
    uint8_t f[MAX_LINES];
    for (int i = 0; i < p.mode[m].nlines; i++) {
      f[i] = lines_worst_format(p.mode[m].lines[i].source, p.mode[m].lines[i].format,
                                (Mode)m);
    }
    Layout l;
    CHECK(layout_compute(&p.mode[m], f, &l));
    for (int i = 0; i < l.n; i++) {
      CHECK(l.g[i].text_x >= l.g[i].gutter_w);
      CHECK(l.g[i].y >= 0 && l.g[i].y + l.g[i].h <= SCREEN_H);
    }
  }
  // Four L lines never fit.
  ModeLayout big;
  settings_mode_layout_default(MODE_UP, &big);
  big.nlines = 4;
  for (int i = 0; i < 4; i++) big.lines[i] = (LineCfg){ SRC_TOTAL, SIZE_L, FMT_MS, 0 };
  uint8_t f4[4] = { FMT_MS, FMT_MS, FMT_MS, FMT_MS };
  Layout l;
  CHECK(!layout_compute(&big, f4, &l) && l.error == LAYOUT_TOO_TALL);

  // Two L lines fit.
  big.nlines = 2;
  CHECK(layout_compute(&big, f4, &l));
  CHECK(l.g[0].y - 0 == SCREEN_H - (l.g[1].y + l.g[1].h));  // centred

  // Phone decode round trip with defaults.
  uint8_t blob[PHONE_CFG_BYTES];
  memset(blob, 0, sizeof(blob));
  blob[0] = PHONE_SETTINGS_VERSION;
  blob[1] = 7;
  blob[2] = 2;  // ghost level
  blob[PHONE_CFG_HEADER + 6] = SRC_TOTAL;  // mode 0, line 0
  PhoneSettings q;
  CHECK(settings_phone_decode(blob, sizeof(blob), &q));
  CHECK(q.reveal_s == 7 && STYLE_GHOST_LEVEL(q.style) == 2);
  CHECK(q.mode[0].nlines == 1 && q.mode[1].nlines > 0);
}

// The size combinations that must fit on 228 px.
static void test_xl_combinations(void) {
  static const uint8_t COMBOS[][4] = {
    { SIZE_XL, SIZE_M,  SIZE_M,  255 },
    { SIZE_XL, SIZE_L,  SIZE_M,  255 },
    { SIZE_XL, SIZE_S,  SIZE_S,  SIZE_S },
    { SIZE_XL, SIZE_M,  SIZE_S,  SIZE_S },
    { SIZE_XL, SIZE_XL, 255,     255 },
  };
  for (unsigned c = 0; c < sizeof(COMBOS) / sizeof(COMBOS[0]); c++) {
    ModeLayout ml;
    memset(&ml, 0, sizeof(ml));
    uint8_t fmts[MAX_LINES];
    for (int i = 0; i < MAX_LINES && COMBOS[c][i] != 255; i++) {
      ml.lines[i] = (LineCfg){ SRC_TOTAL, COMBOS[c][i], FMT_MS, 0 };
      fmts[i] = FMT_MS;
      ml.nlines++;
    }
    Layout l;
    CHECK(layout_compute(&ml, fmts, &l));
  }
}

static void test_auto_format(void) {
  Counter c;
  memset(&c, 0, sizeof(c));
  counter_setup_for(&c, 2 * 86400000LL);
  counter_start(&c, 0, 0);
  CHECK(lines_resolve_format(SRC_REMAINING, FMT_AUTO, &c, 0) == FMT_DHMS);
  // One day gone: the day field goes with it.
  CHECK(lines_resolve_format(SRC_REMAINING, FMT_AUTO, &c, 86400000LL + 1) == FMT_HMS);
  int64_t almost = 2 * 86400000LL - 30 * 60000LL;
  CHECK(lines_resolve_format(SRC_REMAINING, FMT_AUTO, &c, almost) == FMT_MS);

  counter_setup_up(&c, 0);
  counter_start(&c, 0, 0);
  CHECK(lines_resolve_format(SRC_TOTAL, FMT_AUTO, &c, 5000) == FMT_MS);
  CHECK(lines_resolve_format(SRC_TOTAL, FMT_AUTO, &c, 3600000LL) == FMT_HMS);
  CHECK(lines_resolve_format(SRC_TOTAL, FMT_AUTO, &c, 86400000LL) == FMT_DHMS);
  // An explicit choice is never overridden.
  CHECK(lines_resolve_format(SRC_TOTAL, FMT_DHMS, &c, 5000) == FMT_DHMS);
}

// A line that cannot carry its size steps down a whole class, so the box,
// the gutter icon and the label all describe the same thing.
static void test_size_step_down(void) {
  struct { uint8_t asked, fmt, expect; } CASES[] = {
    { SIZE_XL, FMT_MS,   SIZE_XL },
    { SIZE_XL, FMT_HMS,  SIZE_L },
    { SIZE_XL, FMT_DHMS, SIZE_M },
    { SIZE_L,  FMT_DHMS, SIZE_M },
    { SIZE_M,  FMT_DHMS, SIZE_M },
  };
  for (unsigned i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
    ModeLayout ml;
    memset(&ml, 0, sizeof(ml));
    ml.nlines = 1;
    ml.lines[0] = (LineCfg){ SRC_TOTAL, CASES[i].asked, CASES[i].fmt, 0 };
    uint8_t f[1] = { CASES[i].fmt };
    Layout l;
    CHECK(layout_compute(&ml, f, &l));
    CHECK(l.g[0].size == CASES[i].expect);
    CHECK(l.g[0].h == (l.g[0].size == SIZE_XL ? 96
                       : l.g[0].size == SIZE_L ? 62
                       : l.g[0].size == SIZE_M ? 42 : 28));
  }
}

int main(void) {
  test_format();
  test_next_change();
  test_model();
  test_alarms();
  test_lines_and_layout();
  test_xl_combinations();
  test_auto_format();
  test_size_step_down();
  printf("%d passed, %d failed\n", s_pass, s_fail);
  return s_fail ? 1 : 0;
}
