// comm.c - phone settings over AppMessage.
//
// The config page sends one blob when it closes. The watch validates every
// layout with the real fitting rules, keeps the previous layout for a mode
// that does not fit, and reports back what happened.
#include "comm.h"

#include "app.h"
#include "layout.h"
#include "lines.h"
#include "win_main.h"

#define INBOX_SIZE  256
#define OUTBOX_SIZE 128

// ==== VALIDATION ====

static const char *mode_name(Mode m) {
  switch (m) {
    case MODE_UP:       return "count up";
    case MODE_DOWN_FOR: return "count down";
    default:            return "until";
  }
}

// Layouts are checked against the widest format a line can resolve to, so
// an accepted layout also fits once the target grows.
static bool mode_fits(const ModeLayout *ml, Mode m, Layout *out) {
  uint8_t fmts[MAX_LINES];
  for (uint8_t i = 0; i < ml->nlines; i++) {
    fmts[i] = lines_worst_format(ml->lines[i].source, ml->lines[i].format, m);
  }
  return layout_compute(ml, fmts, out);
}

// ==== INBOX ====

// AppMessage only reaches a running app, so a config saved while the timer
// was closed would be lost. The watch asks for the stored settings on
// every launch and the phone replies with its last blob.
static void request_settings(void *data) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_uint8(out, MESSAGE_KEY_CFG_REQ, 1);
  app_message_outbox_send();
}

static void send_status(const char *status) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_cstring(out, MESSAGE_KEY_CFG_STATUS, status);
  app_message_outbox_send();
}

static void apply_settings(const PhoneSettings *incoming) {
  static char status[80];
  static char notice[64];
  PhoneSettings merged = *incoming;
  uint8_t rejected = 0;
  Mode first_bad = MODE_UP;
  Layout probe;
  uint8_t bad_line = 0;
  uint8_t bad_error = 0;

  for (int m = 0; m < MODE_COUNT; m++) {
    if (mode_fits(&merged.mode[m], (Mode)m, &probe)) continue;
    if (rejected == 0) {
      first_bad = (Mode)m;
      bad_line = probe.error_line;
      bad_error = probe.error;
    }
    rejected++;
    merged.mode[m] = g_phone.mode[m];  // keep what was there
  }

  g_phone = merged;
  app_settings_changed();

  if (rejected == 0) {
    send_status("OK");
    win_main_notice("Settings updated");
    return;
  }
  const char *why = (bad_error == LAYOUT_TOO_TALL) ? "too tall" : "line too wide";
  snprintf(status, sizeof(status), "Kept %u layout(s): %s %s (line %u)", rejected,
           mode_name(first_bad), why, (unsigned)(bad_line + 1));
  snprintf(notice, sizeof(notice), "Layout rejected: %s %s", mode_name(first_bad), why);
  send_status(status);
  win_main_notice(notice);
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *cfg = dict_find(iter, MESSAGE_KEY_CFG);
  if (!cfg || cfg->type != TUPLE_BYTE_ARRAY) return;
  PhoneSettings incoming;
  if (!settings_phone_decode(cfg->value->data, cfg->length, &incoming)) {
    send_status("Rejected: unknown config version");
    return;
  }
  apply_settings(&incoming);
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "config dropped: %d", (int)reason);
}

// ==== PUBLIC ====

void comm_open(void) {
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_open(INBOX_SIZE, OUTBOX_SIZE);
  app_timer_register(1500, request_settings, NULL);  // let the link settle
}
