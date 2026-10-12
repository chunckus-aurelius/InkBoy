#!/usr/bin/env python3
"""Check battery latch and power-off ordering of the real power module."""
from host_build import run

CODE = r'''
#include <assert.h>
#include <setjmp.h>
#include <string.h>
#include "fake_game_storage.h"
#include "fake_screen.h"
#include "host_fakes.h"
#include "power.h"

static bool save_ok;
static bool asleep;
static jmp_buf sleep_jump;

bool game_session_ensure_saved(void)
{
    host_trace("ensure_saved");
    return save_ok;
}

static void deep_sleep(void)
{
    asleep = true;
    longjmp(sleep_jump, 1);
}

static void start(void)
{
    host_fakes_reset();
    fake_storage_reset();
    fake_screen_message_ok = true;
    save_ok = true;
    asleep = false;
    host_deep_sleep_hook = deep_sleep;
}

static bool before(const char *first, const char *second)
{
    int a = host_trace_position(first), b = host_trace_position(second);
    return a >= 0 && b > a;
}

int main(void)
{
    /* HIGH is preloaded and the pin configured before releasing a retained hold. */
    start();
    assert(power_enable_battery() == ESP_OK);
    assert(before("gpio_set_level(17,1)", "gpio_config(mask=20000,mode=2,pullup=1)"));
    assert(before("gpio_config(mask=20000", "gpio_hold_dis(17)"));

    /* Never unmount or show OFF after a save failure. */
    start();
    save_ok = false;
    power_off();
    assert(host_trace_has("ensure_saved") && !host_trace_has("esp_sleep_enable_ext0_wakeup") && !asleep);

    start();
    host_wake_enable_error = ESP_FAIL;
    power_off();
    assert(host_trace_has("esp_sleep_enable_ext0_wakeup") && !host_trace_has("game_storage_close") && !asleep);

    start();
    fake_storage.close_error = ESP_FAIL;
    power_off();
    assert(host_trace_has("game_storage_close") && !host_trace_has("screen_message(OFF)") && !asleep);

    start();
    fake_screen_message_ok = false;
    power_off();
    assert(host_trace_has("screen_message(OFF)") && !host_trace_has("gpio_hold_en") && !asleep);

    start();
    if (!setjmp(sleep_jump)) power_off();
    assert(asleep);
    assert(before("ensure_saved", "esp_sleep_enable_ext0_wakeup(0,0)"));
    assert(before("esp_sleep_enable_ext0_wakeup", "rtc_gpio_pullup_en(0)"));
    assert(before("rtc_gpio_pulldown_dis(0)", "game_storage_close"));
    assert(before("game_storage_close", "screen_message(OFF)"));
    assert(before("screen_message(OFF)", "gpio_hold_en(6)"));
    assert(before("gpio_hold_en(6)", "gpio_hold_en(17)"));
    assert(before("gpio_hold_en(17)", "gpio_deep_sleep_hold_en"));
    assert(before("gpio_deep_sleep_hold_en", "game_audio_shutdown"));
    assert(before("game_audio_shutdown", "esp_deep_sleep_start"));
    return 0;
}
'''

run(CODE, modules=['power.c'], fakes=['fake_screen.c', 'fake_menu.c', 'fake_game_storage.c', 'fake_game_audio.c'])
print('PASS: battery latch order and power-off ordering: save, wake source, unmount, OFF screen, pin holds, audio shutdown, sleep; every earlier failure aborts')
