#pragma once
#include "screen.h"

/* Stand-in for main/screen.c that never touches a display driver. */
extern bool fake_screen_failed, fake_screen_close_ok, fake_screen_message_ok;
extern unsigned fake_screen_messages;
extern char fake_screen_last_title[64];
