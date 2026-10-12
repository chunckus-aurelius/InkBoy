#include "fake_menu.h"
#include <assert.h>
#include <stdio.h>

fake_menu_hook_t fake_menu_hook;
unsigned fake_menu_notices;
char fake_menu_notice_title[64];

int menu_choose(const char *title, const char *subtitle, const char *const *items,
                unsigned count, unsigned *selected, int *direction)
{
    assert(fake_menu_hook);
    const char *labels[count];
    for (unsigned i=0;i<count;++i) labels[i]=items[i][0]=='\033' ? items[i]+1 : items[i];
    return fake_menu_hook(title, subtitle, labels, count, selected, direction);
}

void menu_notice(const char *title, const char *line1, const char *line2)
{
    (void)line1; (void)line2;
    ++fake_menu_notices;
    snprintf(fake_menu_notice_title, sizeof(fake_menu_notice_title), "%s", title);
}

bool menu_confirm(const char *title, const char *question, const char *action) {
    const char *items[] = {"CANCEL", action}; unsigned selected = 0;
    return menu_choose(title, question, items, 2, &selected, NULL) == 1;
}
