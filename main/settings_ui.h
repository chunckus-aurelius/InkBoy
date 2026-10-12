#pragma once
#include <stdbool.h>
#include "nvs.h"

void settings_ui_init(nvs_handle_t settings);
void settings_ui_load_display(void);
void settings_ui_load_toggles(void);
bool settings_ui_sound_enabled(void);
void settings_ui_open(void);
