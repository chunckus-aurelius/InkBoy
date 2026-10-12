#!/usr/bin/env python3
"""Dithered mode must reach a valid display state when internal RAM is fragmented."""
from host_build import run

CODE = r'''
#include "fast_display.c"
#include <assert.h>
#include "host_fakes.h"
#include "host_panel.h"

static bool counter_internal_fails, counter_external_fails;
static unsigned counter_internal_attempts, counter_external_attempts;

static bool counters_fail(size_t size, unsigned caps)
{
    if (size != 25004) return false;
    if (caps & MALLOC_CAP_INTERNAL) { ++counter_internal_attempts; return counter_internal_fails; }
    if (caps & MALLOC_CAP_SPIRAM) { ++counter_external_attempts; return counter_external_fails; }
    return false;
}

static uint8_t image[5000];

int main(void)
{
    host_fakes_reset();
    host_panel_reset();
    host_heap_fail_hook = counters_fail;
    ssd1681_handle_t d = (ssd1681_handle_t)1;
    memset(image, 85, sizeof(image));
    assert(sizeof(drive_counters_t) == 25004);
    counter_internal_fails = true;
    assert(!fast_display_full(d, image, DISPLAY_DITHERED));
    assert(owner == d && selected == DISPLAY_DITHERED && drive_counters && !drive_counters->pending_pixels);
    assert(counter_internal_attempts == 1 && counter_external_attempts == 1);
    assert(!memcmp(drive_counters->target, image, sizeof(image)));
    assert(!fast_display_full(d, image, DISPLAY_GRAYSCALE) && !drive_counters);
    counter_external_fails = true;
    unsigned before = host_panel.updates;
    assert(fast_display_full(d, image, DISPLAY_DITHERED) == ESP_ERR_NO_MEM && !owner && !drive_counters);
    assert(host_panel.updates == before); /* no partial reference after allocation failure */
    counter_internal_fails = false; counter_external_fails = false;
    unsigned external_before = counter_external_attempts;
    assert(!fast_display_full(d, image, DISPLAY_DITHERED));
    assert(drive_counters && counter_external_attempts == external_before);
    assert(!fast_display_full(d, image, DISPLAY_GRAYSCALE));
    return 0;
}
'''

run(CODE, modules=['gray_plan.c', 'gray_plan_parallel.c', 'drive_counters.c'], fakes=['host_panel.c'])
print('PASS: dithered internal-first/PSRAM fallback, exact seeded state, complete failure and recovery')
