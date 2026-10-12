#!/usr/bin/env python3
"""Check the controller button mapping, its default wiring and saved overrides in the real input module."""
from host_build import run

CODE = r'''
#include <assert.h>
#include <string.h>
#include "controller.h"
#include "fake_ble_gamepad.h"
#include "fake_screen.h"
#include "fake_usb_gamepad.h"
#include "host_fakes.h"
#include "input.h"

enum { A = 1, B = 2, SELECT = 4, START = 8, RIGHT = 16, LEFT = 32, UP = 64, DOWN = 128 };

static bool no_controller(void) { return false; }

static nvs_handle_t open_settings(unsigned transport)
{
    host_fakes_reset();
    host_nvs_seed("gamepad", "controller", HOST_NVS_U8, &(uint8_t){(uint8_t)transport}, 1);
    fake_usb_reset();
    fake_screen_message_ok = true;
    fake_screen_messages = 0;
    nvs_handle_t handle;
    assert(nvs_open("gamepad", NVS_READWRITE, &handle) == ESP_OK);
    controller_load(handle);
    assert(controller_start() == ESP_OK);
    return handle;
}

static void press(uint8_t mask)
{
    if (controller_transport() == CONTROLLER_USB) fake_usb_report(mask);
    else fake_ble_raw = mask;
}

static void expect_wiring(const uint8_t *gpio_to_button)
{
    for (unsigned gpio = 0; gpio < 8; ++gpio) {
        press((uint8_t)(1u << gpio));
        assert(input_buttons() == gpio_to_button[gpio]);
    }
    press(0xff);
    assert(input_buttons() == 0xff);
    press(0);
    assert(input_buttons() == 0);
}

static void published_wiring_is_the_default(void)
{
    nvs_handle_t handle = open_settings(CONTROLLER_BLUETOOTH);
    input_load_mapping(handle);
    const uint8_t wiring[] = {DOWN, LEFT, UP, RIGHT, SELECT, B, A, START};
    expect_wiring(wiring);
    assert(input_mapping_valid() && host_nvs_commits == 0 && !host_nvs_find("gamepad", "buttons"));
    assert(host_log_count("published controller wiring") == 1);
}

static void usb_uses_the_same_mapping_and_releases_on_disconnect(void)
{
    const uint8_t saved[] = {0x20, 0x10, 0x80, 0x40, 0x02, 0x04, 0x08, 0x01};
    nvs_handle_t handle = open_settings(CONTROLLER_USB);
    assert(controller_transport() == CONTROLLER_USB);
    host_nvs_seed("gamepad", "buttons", HOST_NVS_BLOB, saved, sizeof(saved));
    input_load_mapping(handle);
    const uint8_t wiring[] = {DOWN, RIGHT, LEFT, UP, B, A, START, SELECT};
    assert(!input_connected() && input_buttons() == 0);
    expect_wiring(wiring);
    fake_usb_report(0x01);
    assert(input_connected() && input_buttons() == DOWN && input_report_count() > 0);
    fake_usb_unplug();
    assert(!input_connected() && input_buttons() == 0);
    fake_usb_unplug();
    assert(input_buttons() == 0);
}

static void waiting_for_a_controller(void)
{
    nvs_handle_t handle = open_settings(CONTROLLER_BLUETOOTH);
    input_load_mapping(handle);
    assert(input_wait_controller() && !fake_screen_messages);
    fake_ble_connected = no_controller;
    fake_screen_message_ok = false;
    assert(!input_wait_controller() && fake_screen_messages == 1);
    assert(controller_select(CONTROLLER_USB) == ESP_OK);
    fake_screen_messages = 0;
    assert(!input_wait_controller() && fake_screen_messages == 1);
    fake_usb_report(0);
    assert(input_wait_controller() && fake_screen_messages == 1);
}

static void saved_mapping_wins(void)
{
    const uint8_t saved[] = {0x20, 0x10, 0x80, 0x40, 0x02, 0x04, 0x08, 0x01};
    nvs_handle_t handle = open_settings(CONTROLLER_BLUETOOTH);
    host_nvs_seed("gamepad", "buttons", HOST_NVS_BLOB, saved, sizeof(saved));
    input_load_mapping(handle);
    const uint8_t wiring[] = {DOWN, RIGHT, LEFT, UP, B, A, START, SELECT};
    expect_wiring(wiring);
    assert(host_log_count("published controller wiring") == 0);
}

static void unusable_saved_mapping_falls_back(void)
{
    const uint8_t duplicate[] = {1, 1, 4, 8, 16, 32, 64, 128}, short_blob[] = {0x40, 0x20, 0x10};
    nvs_handle_t handle = open_settings(CONTROLLER_BLUETOOTH);
    const uint8_t wiring[] = {DOWN, LEFT, UP, RIGHT, SELECT, B, A, START};
    host_nvs_seed("gamepad", "buttons", HOST_NVS_BLOB, duplicate, sizeof(duplicate));
    input_load_mapping(handle);
    expect_wiring(wiring);
    host_nvs_seed("gamepad", "buttons", HOST_NVS_BLOB, short_blob, sizeof(short_blob));
    input_load_mapping(handle);
    expect_wiring(wiring);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "default")) published_wiring_is_the_default();
    else if (!strcmp(argv[1], "saved")) saved_mapping_wins();
    else if (!strcmp(argv[1], "fallback")) unusable_saved_mapping_falls_back();
    else if (!strcmp(argv[1], "usb")) usb_uses_the_same_mapping_and_releases_on_disconnect();
    else if (!strcmp(argv[1], "wait")) waiting_for_a_controller();
    else assert(0);
    return 0;
}
'''

run(CODE, modules=['input.c', 'controller.c'], fakes=['fake_screen.c', 'fake_ble_gamepad.c', 'fake_usb_gamepad.c'],
    scenarios=[[name] for name in ('default', 'saved', 'fallback', 'usb', 'wait')])
print('PASS: published wiring is the default mapping; a valid saved mapping wins; unusable saved mappings fall back; USB reports use the same mapping and release on disconnect; waiting accepts either transport')
