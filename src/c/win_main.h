// win_main.h - the counter screen.
#pragma once

void win_main_create_and_push(void);
void win_main_destroy(void);

void win_main_refresh(void);    // values or state changed
void win_main_relayout(void);   // settings or mode changed
void win_main_notice(const char *text);
