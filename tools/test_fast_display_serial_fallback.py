#!/usr/bin/env python3
"""When the grayscale worker cannot be allocated, the fast display must render identically via the serial planner."""
from host_build import run

CODE = r'''
#include "fast_display.c"
#include <assert.h>
#include <stdint.h>
#include "gray_plan_parallel.h"
#include "host_fakes.h"
#include "host_panel.h"

static uint64_t digest;
static unsigned scan_count;
static uint8_t binary[5000], game[5760];

static void mix(const void *data, size_t size)
{
    const uint8_t *bytes = data;
    for (size_t i = 0; i < size; ++i) digest = (digest ^ bytes[i]) * 1099511628211ull;
}

static void record_scan(const uint8_t *black, const uint8_t *white)
{
    mix(black, SSD1681_FRAME_BYTES);
    mix(white, SSD1681_FRAME_BYTES);
    ++scan_count;
}

static uint32_t seed = 12345;
static uint8_t random_byte(void) { seed = seed * 1664525u + 1013904223u; return (uint8_t)(seed >> 24); }

static void new_picture(unsigned frame)
{
    if (frame % 7 == 3) memset(game, (int)(frame % 4) * 85, sizeof(game));
    else for (unsigned i = 0; i < sizeof(game); ++i) game[i] = frame % 3 ? random_byte() : (uint8_t)(game[i] ^ random_byte());
}

/* Variant 0: stock policy. 1: pair mode with every HOLD. 2: history/selective drive on. */
static uint64_t play(unsigned variant)
{
    digest = 14695981039346656037ull;
    scan_count = 0;
    seed = 12345;
    host_panel_reset();
    host_panel.require_gray_frame_rate = true;
    host_panel.before_scan = record_scan;
    gray_config_t config = GRAY_CONFIG_DEFAULT;
    gray_adaptive_t adaptive = GRAY_ADAPTIVE_DEFAULT;
    if (variant == 2) { adaptive.history_mode = 3; adaptive.weak_wl = 1; }
    assert(!fast_display_set_gray_config(&config) && !fast_display_set_gray_adaptive(&adaptive));
    assert(!fast_display_set_gray_immediate(variant == 1) && !fast_display_set_gray_holds(GRAY_HOLD_ALL));
    memset(binary, 255, sizeof(binary));
    memset(game, 0, sizeof(game));
    fast_display_set_gray_game(game);
    ssd1681_handle_t d = (ssd1681_handle_t)1;
    assert(!fast_display_full(d, binary, DISPLAY_GRAYSCALE));
    for (unsigned frame = 0; frame < 120; ++frame) {
        if (frame % 4 == 0) new_picture(frame);
        assert(!fast_display_update(d, binary));
        mix(&gray_plan->pending, sizeof(gray_plan->pending));
        unsigned scans = fast_display_gray_last_scans();
        mix(&scans, sizeof(scans));
    }
    while (fast_display_pending(d)) assert(!fast_display_update(d, binary));
    for (unsigned i = 0; i < GRAY_PIXELS; ++i) mix(&gray_plan->pixel[i], sizeof(gray_plan->pixel[i]));
    assert(scan_count >= 120);
    assert(!fast_display_full(d, binary, DISPLAY_DITHERED)); /* Frees the grayscale workspace. */
    return digest;
}

int main(void)
{
    host_fakes_reset();
    for (unsigned variant = 0; variant < 3; ++variant) {
        gray_plan_parallel_host_init_fails = false;
        host_log_clear();
        uint64_t accelerated = play(variant);
        assert(host_log_count("PLAYER_GRAY_MEMORY,accelerated,1") == 1);
        gray_plan_parallel_host_init_fails = true;
        host_log_clear();
        uint64_t serial = play(variant);
        assert(host_log_count("PLAYER_GRAY_MEMORY,accelerated,0") == 1 && !gray_plan_parallel_acceleration_bytes());
        assert(accelerated == serial);
    }
    return 0;
}
'''

run(CODE, modules=['gray_plan.c', 'gray_plan_parallel.c', 'drive_counters.c'], fakes=['host_panel.c'])
print('PASS: fast display renders identical scans and planner state with and without the grayscale worker, stock, pair-mode and history policies')
