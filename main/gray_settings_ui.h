#pragma once
#include <stdbool.h>
#include "display_preset.h"

void gray_settings_ui_load(void);
void gray_settings_ui_open(void);
void gray_settings_ui_transition_test(void);
/* Applies a grayscale preset to the display backend and stores it as the active state. */
esp_err_t gray_settings_ui_activate(const display_preset_t *preset, bool modified);
