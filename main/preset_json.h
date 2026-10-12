#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "display_preset.h"

#define PRESET_FILE_MAX 8192u
#define PRESET_JSON_MAX 2048u

typedef enum {
    PRESET_JSON_OK,
    PRESET_JSON_TOO_LARGE,
    PRESET_JSON_MALFORMED,
    PRESET_JSON_WRONG_FORMAT,
    PRESET_JSON_WRONG_MODE,
    PRESET_JSON_MISSING_KEY,
    PRESET_JSON_BAD_VALUE,
} preset_json_status_t;

/* Canonical text of `inkboy-preset-v1`; false when invalid or `size` is too small. */
bool preset_json_encode(const display_preset_t *preset, char *json, size_t size, size_t *length);
/* json[length] must be a readable NUL so trailing garbage can be refused.
 * Unknown keys are ignored; a missing or out-of-range value rejects the file. */
preset_json_status_t preset_json_decode(const char *json, size_t length, display_mode_t mode, display_preset_t *preset);
const char *preset_json_status_name(preset_json_status_t status);
