#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "display_mode.h"

typedef struct {
    display_mode_t mode;
    uint8_t dither_black_pulses, dither_white_pulses;
    bool show_fps;
} display_config_t;

display_config_t display_config_get(void);
void display_config_set(const display_config_t *config);
const char *display_config_mode_name(void);
