// test_cfg_roundtrip.c - decodes the byte blob produced by src/pkjs and
// checks that every default layout fits the screen. Run via `make -C test`.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/c/layout.h"
#include "../src/c/lines.h"
#include "../src/c/settings.h"

int main(int argc, char **argv) {
  uint8_t blob[PHONE_CFG_BYTES];
  memset(blob, 0, sizeof(blob));
  if (argc - 1 != PHONE_CFG_BYTES) {
    printf("FAIL cfg: got %d bytes, expected %d\n", argc - 1, PHONE_CFG_BYTES);
    return 1;
  }
  for (int i = 0; i < PHONE_CFG_BYTES; i++) blob[i] = (uint8_t)atoi(argv[i + 1]);

  PhoneSettings p;
  if (!settings_phone_decode(blob, sizeof(blob), &p)) {
    printf("FAIL cfg: decode rejected the phone blob\n");
    return 1;
  }
  int fail = 0;
  for (int m = 0; m < MODE_COUNT; m++) {
    uint8_t f[MAX_LINES];
    for (int i = 0; i < p.mode[m].nlines; i++) {
      f[i] = lines_worst_format(p.mode[m].lines[i].source, p.mode[m].lines[i].format,
                                (Mode)m);
    }
    Layout l;
    if (!layout_compute(&p.mode[m], f, &l)) {
      printf("FAIL cfg: mode %d layout error %d line %d\n", m, l.error, l.error_line);
      fail = 1;
    }
  }
  printf("cfg round trip: %s (reveal %us, style %u)\n", fail ? "FAILED" : "ok",
         p.reveal_s, p.style);
  return fail;
}
