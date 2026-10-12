#pragma once
#include <stdbool.h>
/* Back gestures replace navigation rows while preserving action indices. */
#define MENU_BACK "\033BACK"
#define MENU_CANCEL "\033CANCEL"

int menu_choose(const char *title, const char *subtitle, const char *const *items,
                unsigned count, unsigned *selected, int *direction);
void menu_notice(const char *title, const char *line1, const char *line2);
bool menu_confirm(const char *title, const char *question, const char *action);
