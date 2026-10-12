#pragma once
#include <stdbool.h>
#include "display_preset.h"

void preset_ui_open(void);
/* Stores the preset as the active state and applies it live; `modified` marks edits made since it was loaded. */
esp_err_t preset_ui_activate(const display_preset_t *preset, bool modified);
