// format.h - turns a value into LCD cells. Pure C (no pebble.h).
//
// Rules implemented here (see PLAN.md, "Digit renderer" and "Refresh"):
// - fixed format per line; digits are monospaced cells
// - truncate to the display resolution, never round
// - digits below the resolution show as '_'
// - leading zero fields are ghosted (drawn as dim 8s), layout never shifts
// - values past the format saturate with '>' in the sign cell
// - no data: digit cells show '-'
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "settings.h"

typedef enum { CELL_DIGIT = 0, CELL_SIGN, CELL_SEP } CellKind;

#define CF_GHOST 0x01   // draw only the ghost segments
#define CF_DIM   0x02   // draw lit segments in the dim colour (editor)

typedef struct {
  char    ch;     // '0'-'9', '-', '_', 'd', '+', '>', ' ', ':', '.'
  uint8_t kind;   // CellKind
  uint8_t flags;  // CF_*
} Cell;

#define MAX_CELLS 14

typedef enum { COL_FG = 0, COL_RED, COL_GREEN } ColClass;

typedef struct {
  uint8_t n;
  uint8_t color;  // ColClass for all lit cells
  Cell    cells[MAX_CELLS];
} LineText;

// Wide cells (digits, sign, 'd') and narrow cells (':' '.') of a format.
void format_counts(Format f, uint8_t *wide, uint8_t *narrow);

// value: milliseconds (FMT_HM: ms of the day; FMT_COUNT2: a plain count).
// res_ms: display resolution (100, 1000, 10000, 60000).
void format_line(LineText *out, Format f, int64_t value, bool has, int64_t res_ms,
                 uint8_t color);

bool format_text_equal(const LineText *a, const LineText *b);

// ms until the displayed text of a value changes, given its rate of change
// (+1 rising, -1 falling, 0 static) and display resolution. <0 = never.
int64_t format_next_change_ms(int64_t value, int8_t rate, int64_t res_ms);

// Adaptive resolution from a measured value and the generic thresholds.
int64_t format_choose_res(int64_t measured, uint32_t coarse_ms, uint32_t fine_ms,
                          int64_t finest_ms);

// ms until format_choose_res() would pick a different resolution. <0 = never.
int64_t format_res_change_ms(int64_t measured, int8_t rate, uint32_t coarse_ms,
                             uint32_t fine_ms);
