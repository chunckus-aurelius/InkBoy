#!/usr/bin/env python3
"""Check the exclusive Bluetooth/USB controller transport in the real controller module."""
from host_build import run

CODE = r'''
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include "esp_log.h"
#include "controller.h"
#include "fake_ble_gamepad.h"
#include "fake_usb_gamepad.h"
#include "host_fakes.h"

static nvs_handle_t handle;

static int sentinel_log(const char *format, va_list args) { (void)format; (void)args; return 42; }

static int call_logger(vprintf_like_t logger, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int written = logger(format, args);
    va_end(args);
    return written;
}

static void boot(int saved)
{
    host_fakes_reset();
    fake_usb_reset();
    fake_ble_started = false;
    fake_ble_starts = fake_ble_stops = 0;
    fake_ble_start_error = fake_ble_stop_error = ESP_OK;
    if (saved >= 0) host_nvs_seed("gamepad", "controller", HOST_NVS_U8, &(uint8_t){(uint8_t)saved}, 1);
    assert(nvs_open("gamepad", NVS_READWRITE, &handle) == ESP_OK);
    controller_load(handle);
}

static void saved_ble(void)
{
    boot(-1);
    assert(controller_transport() == CONTROLLER_BLUETOOTH && controller_start() == ESP_OK);
    assert(controller_running() && fake_ble_started && !fake_usb_starts);
    assert(host_log_count("PLAYER_CONTROLLER,transport,ble,result,ESP_OK") == 1);
}

static void saved_usb(void)
{
    boot(CONTROLLER_USB);
    assert(controller_transport() == CONTROLLER_USB && controller_start() == ESP_OK);
    assert(controller_running() && usb_gamepad_running() && !fake_ble_starts);
    assert(host_log_count("PLAYER_CONTROLLER,transport,usb,result,ESP_OK") == 1);
}

static void saved_value_out_of_range(void)
{
    boot(2);
    assert(controller_transport() == CONTROLLER_BLUETOOTH && controller_start() == ESP_OK && fake_ble_started);
}

static void boot_falls_back_when_usb_fails(void)
{
    boot(CONTROLLER_USB);
    fake_usb_start_error = ESP_FAIL;
    assert(controller_start() == ESP_OK);
    assert(controller_transport() == CONTROLLER_BLUETOOTH && fake_ble_started && controller_running());
    assert(host_nvs_u8("gamepad", "controller") == CONTROLLER_USB);
    assert(host_log_count("PLAYER_CONTROLLER,transport,usb,result,ESP_ERR") == 1);
    assert(controller_select(CONTROLLER_BLUETOOTH) == ESP_OK && host_nvs_u8("gamepad", "controller") == CONTROLLER_BLUETOOTH);
    assert(fake_ble_starts == 1 && !fake_ble_stops);
}

static void bluetooth_stops_before_usb_starts(void)
{
    boot(-1);
    assert(controller_start() == ESP_OK);
    host_trace_clear();
    assert(controller_select(CONTROLLER_USB) == ESP_OK);
    assert(host_trace_position("ble_stop") >= 0 && host_trace_position("ble_stop") < host_trace_position("usb_start"));
    assert(!fake_ble_started && usb_gamepad_running() && controller_transport() == CONTROLLER_USB && controller_running());
    assert(host_nvs_u8("gamepad", "controller") == CONTROLLER_USB);
    assert(host_log_count("PLAYER_CONTROLLER,transport,usb,result,ESP_OK") == 1);
    assert(controller_select(CONTROLLER_USB) == ESP_OK && fake_usb_starts == 1 && fake_ble_stops == 1);
}

static void usb_stops_before_bluetooth_starts(void)
{
    boot(CONTROLLER_USB);
    assert(controller_start() == ESP_OK);
    host_trace_clear();
    assert(controller_select(CONTROLLER_BLUETOOTH) == ESP_OK);
    assert(host_trace_position("usb_stop") >= 0 && host_trace_position("usb_stop") < host_trace_position("ble_start"));
    assert(fake_ble_started && !usb_gamepad_running() && controller_transport() == CONTROLLER_BLUETOOTH);
    assert(host_nvs_u8("gamepad", "controller") == CONTROLLER_BLUETOOTH);
    assert(host_log_count("PLAYER_CONTROLLER,transport,ble,result,ESP_OK") == 1);
}

static void usb_stop_retries_then_gives_up(void)
{
    boot(CONTROLLER_USB);
    assert(controller_start() == ESP_OK);
    fake_usb_stop_timeouts = 2;
    assert(controller_select(CONTROLLER_BLUETOOTH) == ESP_OK && fake_usb_stops == 3 && fake_ble_started);

    boot(CONTROLLER_USB);
    assert(controller_start() == ESP_OK);
    fake_usb_stop_timeouts = 5;
    assert(controller_select(CONTROLLER_BLUETOOTH) == ESP_ERR_TIMEOUT);
    assert(fake_usb_stops == 3 && !fake_ble_starts && controller_running() && controller_transport() == CONTROLLER_USB);
    assert(host_nvs_u8("gamepad", "controller") == CONTROLLER_USB && host_log_count("PLAYER_CONTROLLER,transport,ble,result,ESP_ERR") == 1);
    fake_usb_stop_timeouts = 0;
    assert(controller_select(CONTROLLER_BLUETOOTH) == ESP_OK && fake_ble_started);
}

static void failed_start_restores_the_previous_transport(void)
{
    boot(-1);
    assert(controller_start() == ESP_OK);
    fake_usb_start_error = ESP_FAIL;
    assert(controller_select(CONTROLLER_USB) == ESP_FAIL);
    assert(controller_running() && fake_ble_started && fake_ble_starts == 2 && controller_transport() == CONTROLLER_BLUETOOTH);
    assert(host_nvs_u8("gamepad", "controller") == CONTROLLER_BLUETOOTH);
    fake_ble_start_error = ESP_FAIL;
    assert(controller_select(CONTROLLER_USB) == ESP_FAIL && !controller_running() && !controller_connected() && !controller_raw());
    assert(host_nvs_u8("gamepad", "controller") == CONTROLLER_BLUETOOTH);
    fake_ble_start_error = fake_usb_start_error = ESP_OK;
    assert(controller_select(CONTROLLER_BLUETOOTH) == ESP_OK && controller_running() && fake_ble_started);
}

static void unsaved_choice_is_not_applied(void)
{
    boot(-1);
    assert(controller_start() == ESP_OK);
    host_nvs_set_error = ESP_FAIL;
    assert(controller_select(CONTROLLER_USB) == ESP_FAIL);
    assert(!fake_usb_starts && controller_running() && fake_ble_started && controller_transport() == CONTROLLER_BLUETOOTH);
}

static void console_is_discarded_while_usb_runs(void)
{
    boot(CONTROLLER_USB);
    vprintf_like_t original = esp_log_set_vprintf(sentinel_log);
    assert(controller_start() == ESP_OK);
    vprintf_like_t during = esp_log_set_vprintf(sentinel_log);
    assert(during != sentinel_log && during != original);
    assert(call_logger(during, "lost %d", 1) == 0);
    esp_log_set_vprintf(during);
    fake_usb_release_console();
    assert(controller_select(CONTROLLER_BLUETOOTH) == ESP_OK);
    assert(esp_log_set_vprintf(original) == sentinel_log);
}

static void usb_reports_map_and_release(void)
{
    boot(CONTROLLER_USB);
    assert(controller_start() == ESP_OK);
    assert(!controller_connected() && controller_raw() == 0 && controller_reports() == 0);
    fake_usb_report(0x41);
    assert(controller_connected() && controller_raw() == 0x41 && controller_reports() == 1);
    fake_usb_report(0x00);
    assert(controller_connected() && controller_raw() == 0 && controller_reports() == 2);
    fake_usb_report(0x80);
    fake_usb_unplug();
    fake_usb_unplug();
    assert(!controller_connected() && controller_raw() == 0);
    fake_usb_report(0x08);
    assert(controller_connected() && controller_raw() == 0x08);
    assert(controller_select(CONTROLLER_BLUETOOTH) == ESP_OK && !controller_raw());
    fake_ble_raw = 0x22;
    assert(controller_connected() && controller_raw() == 0x22);
}

static void usb_manual_toggle(void) {
    boot(CONTROLLER_USB);assert(controller_start()==ESP_OK);
    fake_usb_report(1);assert(controller_connected() && controller_status()==CONTROLLER_USB_CONNECTED);
    unsigned writes=host_nvs_u8("gamepad","controller");
    assert(controller_toggle_usb()==ESP_OK && !usb_gamepad_running() && !controller_connected());
    assert(controller_transport()==CONTROLLER_USB && controller_running() && controller_status()==CONTROLLER_USB_PC);
    assert(host_nvs_u8("gamepad","controller")==writes && fake_ble_starts==0);
    assert(controller_toggle_usb()==ESP_OK && usb_gamepad_running() && controller_status()==CONTROLLER_USB_WAIT);
    fake_usb_report(2);assert(controller_raw()==2 && host_nvs_u8("gamepad","controller")==writes);
    fake_usb_stop_timeouts=3;assert(controller_toggle_usb()==ESP_ERR_TIMEOUT && usb_gamepad_running());
    fake_usb_stop_timeouts=0;assert(controller_toggle_usb()==ESP_OK && !usb_gamepad_running());
    assert(controller_select(CONTROLLER_BLUETOOTH)==ESP_OK && fake_ble_started);
    assert(controller_toggle_usb()==ESP_OK && fake_ble_started && !usb_gamepad_running());
}
static void boot_serial_bypass(void) {
    boot(CONTROLLER_USB);controller_request_console();assert(controller_start()==ESP_OK);
    assert(!usb_gamepad_running() && controller_running() && controller_transport()==CONTROLLER_USB);
    assert(host_nvs_u8("gamepad","controller")==CONTROLLER_USB);
    assert(controller_toggle_usb()==ESP_OK && usb_gamepad_running());
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    static const struct { const char *name; void (*run)(void); } cases[] = {
        {"manual_toggle",usb_manual_toggle}, {"boot_console",boot_serial_bypass}, {"saved_ble", saved_ble}, {"saved_usb", saved_usb}, {"range", saved_value_out_of_range},
        {"fallback", boot_falls_back_when_usb_fails}, {"ble_to_usb", bluetooth_stops_before_usb_starts},
        {"usb_to_ble", usb_stops_before_bluetooth_starts}, {"stop_timeout", usb_stop_retries_then_gives_up},
        {"start_failure", failed_start_restores_the_previous_transport}, {"save_failure", unsaved_choice_is_not_applied},
        {"console", console_is_discarded_while_usb_runs}, {"reports", usb_reports_map_and_release},
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        if (!strcmp(argv[1], cases[i].name)) { cases[i].run(); return 0; }
    assert(0);
}
'''

run(CODE, modules=['controller.c'], fakes=['fake_ble_gamepad.c', 'fake_usb_gamepad.c'],
    scenarios=[[name] for name in ('manual_toggle', 'boot_console', 'saved_ble', 'saved_usb', 'range', 'fallback', 'ble_to_usb', 'usb_to_ble', 'stop_timeout',
                                   'start_failure', 'save_failure', 'console', 'reports')])
print('PASS: boot starts the saved transport, one transport at a time in the right order, USB stop retries and timeout, rollback, '
      'unsaved choice, console redirect and restore, USB report mapping and disconnect release')
