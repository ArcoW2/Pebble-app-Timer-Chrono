# Host tests for the pure modules, plus a compile check of the watch code
# against a mock pebble.h. Not part of the Pebble build.
CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O1
PURE = ../src/c/model.c ../src/c/format.c ../src/c/lines.c \
       ../src/c/alarm_calc.c ../src/c/layout.c ../src/c/settings.c
WATCH = $(filter-out $(PURE), $(wildcard ../src/c/*.c))

all: test check cfg

test: test_main.c $(PURE)
	$(CC) $(CFLAGS) -o test_runner test_main.c $(PURE)
	./test_runner

cfg: test_cfg_roundtrip.c $(PURE)
	$(CC) $(CFLAGS) -o cfg_runner test_cfg_roundtrip.c $(PURE)
	./cfg_runner $$(node encode_defaults.js)

# Type-checks the Pebble-only modules without the SDK.
check:
	@for f in $(WATCH); do \
	  $(CC) -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -fsyntax-only \
	    -Imock -I../src/c $$f || exit 1; \
	done
	@echo "watch modules: syntax ok"

clean:
	rm -f test_runner cfg_runner
