#pragma once
#include "display_preset.h"

#define PRESET_LIST_MAX 16u
#define PRESET_FILE_NAME_MAX 31u

typedef enum {
    PRESET_STORE_OK,
    PRESET_STORE_NO_CARD,
    PRESET_STORE_IO,
    PRESET_STORE_INVALID,
    PRESET_STORE_NOT_FOUND,
    PRESET_STORE_FULL,
} preset_store_status_t;

typedef struct {
    char file[PRESET_FILE_NAME_MAX + 1];
    char name[PRESET_NAME_MAX + 1];
} preset_entry_t;

/* Valid presets of one mode, sorted by name, at most PRESET_LIST_MAX. Invalid
 * files are logged and skipped, and a missing directory is an empty list. */
preset_store_status_t preset_store_list(display_mode_t mode, preset_entry_t entries[PRESET_LIST_MAX], unsigned *count);
preset_store_status_t preset_store_load(display_mode_t mode, const char *file, display_preset_t *preset);
/* Writes `source` as user-N.json named "User N" for the lowest free N. */
preset_store_status_t preset_store_save_new(const display_preset_t *source, display_preset_t *saved);
preset_store_status_t preset_store_overwrite(const display_preset_t *preset);
