#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "display_mode.h"
#include "esp_err.h"
#include "gray_profile.h"

#define PRESET_NAME_MAX 20u
#define PRESET_DEFAULT_NAME "Default"

/* Only the settings of `mode` are meaningful; the other half is zero. */
typedef struct {
    display_mode_t mode;
    char name[PRESET_NAME_MAX + 1];
    uint8_t black_pulses, white_pulses;
    gray_profile_t gray;
} display_preset_t;

/* Printable ASCII without quotes or backslashes, no edge spaces, 1..PRESET_NAME_MAX. */
bool display_preset_name_valid(const char *name);
bool display_preset_is_default_name(const char *name);
bool display_preset_valid(const display_preset_t *preset);
bool display_preset_same_settings(const display_preset_t *a, const display_preset_t *b);

const display_preset_t *display_preset_default(display_mode_t mode);

/* Active state lives in NVS namespace "inkboy", so no card is needed at boot. Always leaves valid
 * state; returns the first storage error other than a fresh device. */
esp_err_t display_preset_load_active(void);
const display_preset_t *display_preset_active(display_mode_t mode);
bool display_preset_active_modified(display_mode_t mode);
bool display_preset_active_is_user(display_mode_t mode);
/* Persists values, name and flag together; memory changes only on success. */
esp_err_t display_preset_set_active(const display_preset_t *preset, bool modified);
void display_preset_active_label(display_mode_t mode, char *label, size_t size);
