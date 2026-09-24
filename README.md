# Timer / Chrono — Pebble Time 2

A timer and chronograph for the Pebble Time 2 (emery, 200×228) where count
up and count down never blur into each other, every button shows what it
does before you press it, and the display only redraws when something
visibly changes.

Written in Pebble C. The phone is used for one thing only: a config page
for layouts, colours and the tap-reveal timeout. Timing, laps, alarms,
setup and presets all run on the watch, with or without a phone nearby.

The design plan lives in a separate document (the one this project was
built from). Drop your exported copy next to this file as `PLAN.md` if you
want it in the repo; the sections below describe what was actually built
and where it differs.

## Build in the Pebble Cloud Editor

1. Zip the project (or import this folder) and create a new project from it.
2. Confirm the dependency `pebble-clay` (`^1.0.4`) is present — the
   config page uses it. It is already in `package.json`.
3. Target platform is `emery` only (`package.json` → `pebble.targetPlatforms`).
4. Build and install. On first run the app opens the setup screen.

Locally: `pebble build && pebble install --emulator emery` works the same
way, since `wscript` is the stock app build with `src/c/**/*.c`.

## Using it

| Screen | What it does |
| --- | --- |
| Main | Your configured lines, the mode arrow (bottom-left), hint column (right) |
| Laps | Lap number, lap time, delta vs the prior lap, total; fastest starred |
| Setup | Mode choice (↑ / ↓ for / ↓ until), then the value |
| Editor | Same editor for a running count down's full value |
| Recents | The last five settings, one press to reuse |
| Settings | Thresholds, alarms and their output, lap feedback |

Main screen buttons, per state:

| State | Up | Long Up | Select | Long Select | Down |
| --- | --- | --- | --- | --- | --- |
| Idle | recents | settings | start | setup | laps |
| Running ↑ | lap | — | pause | stop | laps |
| Running ↓ for | lap | edit value | pause | stop | laps |
| Running ↓ until | lap | edit end time | — (no pause) | stop | laps |
| Paused | lap | — | resume | stop | laps |
| Stopped | — | settings | restart | reset | laps |

Back exits; the counter keeps running. Laps screen: Up/Down scroll,
Select or Back returns.

Touch: tap while running reveals full precision for a few seconds
(changes nothing); horizontal swipe switches main ↔ laps; in the editor a
tap selects a field and a circular swipe changes it, clockwise up, with
bigger steps the faster you turn.

## How it is put together

```
src/c/
  model.c        counter: modes, states, pause, laps, active clock   [pure]
  format.c       values to LCD cells, truncation, ghosting, timing   [pure]
  lines.c        line sources, format resolution, resolution choice  [pure]
  alarm_calc.c   which alarm is next                                 [pure]
  layout.c       line boxes, vertical centring, digit fitting        [pure]
  settings.c     defaults and phone-config decoding                  [pure]
  segment.c      seven-segment renderer
  icons.c        icons drawn with primitives (they scale)
  draw.c         polygon and thick-line helpers
  colors.c       palette indices to GColor, per mode
  buttons.c      press engine (short / long / repeat) and hint column
  touchin.c      taps, swipes, circular swipes
  alarms.c       alarm timing, wakeups, missed alarms, output
  comm.c         AppMessage: receive settings, validate, report back
  app.c          global state, storage, recents, the change flow
  win_*.c        main, laps, editor, recents, settings screens
  main.c         lifecycle
src/pkjs/
  index.js       Clay, encoding, send on close
  config.js      the config page
test/            host tests and a compile check (not part of the build)
```

The six modules marked `[pure]` have no `pebble.h` dependency, which is
what makes them testable on a laptop. Everything with timing rules in it
lives there.

Key mechanics:

- **Timestamps, not ticks.** A counter is `start_ms`, `accum_ms`,
  `target_ms` and lap durations. Nothing runs while the app is closed;
  values are derived from `now` when it opens.
- **Active time.** Wall time minus paused time. Laps store durations in
  active time, so a pause inside a lap is excluded by construction and no
  pause log is needed.
- **Redraws.** For every running line the code computes when its displayed
  text next changes and sets one `app_timer` for the earliest. On fire it
  formats, diffs the strings and only marks the layer dirty if something
  actually changed.
- **Alarms.** Computed from the counter, never stored as a schedule. In the
  foreground one timer fires the next; on exit the earliest becomes a
  wakeup three seconds early, so the app is running again before the alarm
  is due. An overdue alarm is reported as "Missed end at 14:32".
- **Settings split.** Layouts, colours and the reveal timeout come from the
  phone on config close; the watch validates each layout with the real
  fitting rules, keeps the previous one if it does not fit, and reports
  back. Everything else is edited on the watch.

## Tests

```
make -C test          # unit tests + compile check + config round trip
```

- `test_main.c` — 72 assertions over the pure modules: pause-excluded laps,
  lap roll-off past 99, truncation and `_` placeholders, ghosting,
  saturation, redraw timing, alarm ordering, layout centring and rejection.
- `check` — compiles every watch module against `test/mock/pebble.h`, a mock
  header written from the PebbleOS applib declarations. It catches type and
  signature mistakes without the SDK. The real build never sees it.
- `cfg` — encodes the default settings with the actual `src/pkjs` code
  (via node) and decodes them with the C decoder, then checks that every
  default layout fits.

## Decisions taken while building

These were not in the plan or were left open there:

- **Laps screen buttons.** The plan has Down meaning "next screen", but the
  list needs scrolling. Here Up/Down scroll and Select or Back returns.
- **Ghosting is a setting** (phone config), since the plan called the LCD
  ghost-8 look a style option.
- **Every time format has a leading sign cell**, used for the overrun `+`
  and the overflow `>`. It costs one digit of width and keeps saturation
  honest in all formats.
- **Layout validation uses the widest format** a line could resolve to, so
  an accepted layout still fits when the target grows past an hour or a day.
- **Until-mode running edit** resolves the new end time against the
  counter's start, not against now. That is what makes "earlier than the
  time already elapsed" possible, and therefore the stop-on-confirm rule.
- **Delta labels are ASCII** ("vsBEST"), because the system font may not
  have Δ.
- **Segment geometry is computed per draw** instead of cached. The maths is
  a handful of integer operations per segment; caching can come back if
  profiling asks for it.
- **Alarm settings are per mode** (three sets), as in the plan, driven by
  one row table so the settings screen stays small.
- **Presets are the recents list** (last five). Named presets are not built.

## Still to verify on the device

- Wakeup spacing in practice: the 1-minute rule is system wide, so a busy
  watch may push a wakeup earlier. The code retries up to four times,
  65 s earlier each time.
- `DOUBLE_TAP_WINDOW_MS` (300 ms in `win_editor.c`) against the system's
  real double-tap timing, and whether the app sees both taps at all.
- Speaker output: `speaker_play_notes` and `speaker_is_muted` exist in the
  SDK, but volume and note choice want tuning by ear.
- Heap use with four L lines, and whether the redraw cost needs the
  per-line bitmap cache the plan mentions.
- Whether `pebble-clay` behaves in the current phone app; if not, the
  config page can be swapped for a plain hosted HTML page — `index.js`
  only needs `getSettings` to hand back the same keys.

## Not built (deliberately)

Multiple counters. The model, the alarm queue and the storage budget are
already shaped for it: counters are independent data, and the overview
screen plus a focus switch is the only real addition.
