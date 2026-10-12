#pragma once
#include "menu.h"

/* Scripted stand-in for main/menu.c: the hook plays the user's choices. */
typedef int (*fake_menu_hook_t)(const char *title, const char *subtitle, const char *const *items,
                                unsigned count, unsigned *selected, int *direction);
extern fake_menu_hook_t fake_menu_hook;
extern unsigned fake_menu_notices;
extern char fake_menu_notice_title[64];
