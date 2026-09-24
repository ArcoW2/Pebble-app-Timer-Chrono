// alarms.h - alarm timing, wakeups and output (vibration / sound).
#pragma once

#include <pebble.h>
#include <stdbool.h>

// At launch: cancel stale wakeups, report a missed alarm, arm the next one.
void alarms_start(bool from_wakeup);

// After any change to the counter or the alarm settings.
void alarms_reschedule(void);

// At exit: turn the next alarm into a wakeup.
void alarms_stop_and_schedule_wakeup(void);

// Any button or touch: postpones the auto-exit after a wakeup launch.
void alarms_note_interaction(void);

void alarms_lap_feedback(void);
void alarms_clamp_tick(void);   // short tick when an edit hits its limit
void alarms_click_tick(void);   // very short feedback on a button press
