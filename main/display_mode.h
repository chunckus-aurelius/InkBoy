#pragma once

/* Stored as a u8 in NVS; keep the values stable. */
typedef enum {
    DISPLAY_DITHERED,
    DISPLAY_GRAYSCALE,
    DISPLAY_MODE_COUNT,
} display_mode_t;
