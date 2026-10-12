#pragma once
#include <stdatomic.h>
#include <stdint.h>
#include "gb_emulator.h"

/* Single producer/consumer; the middle slot is exchanged atomically so each side owns its own buffer. No frame queue. */
typedef struct {
    uint8_t pixels[3][GB_PACKED_BYTES];
    _Atomic unsigned middle;
    unsigned front, back;
} latest_frame_t;

/* Initialize only while both tasks are stopped or excluded by core_lock. */
void latest_frame_seed(latest_frame_t *f, const uint8_t *pixels);
void latest_frame_publish(latest_frame_t *f, const uint8_t *pixels);
void latest_frame_copy(latest_frame_t *f, uint8_t *pixels);
