#include "fake_ble_gamepad.h"
#include "host_fakes.h"

bool (*fake_ble_connected)(void);
uint8_t fake_ble_raw;
esp_err_t fake_ble_start_error, fake_ble_stop_error;
unsigned fake_ble_starts, fake_ble_stops;
bool fake_ble_started;

esp_err_t ble_gamepad_start(void)
{
    host_trace("ble_start");
    ++fake_ble_starts;
    if (fake_ble_start_error) return fake_ble_start_error;
    fake_ble_started = true;
    return ESP_OK;
}

esp_err_t ble_gamepad_stop(void)
{
    host_trace("ble_stop");
    ++fake_ble_stops;
    if (fake_ble_stop_error) return fake_ble_stop_error;
    fake_ble_started = false;
    return ESP_OK;
}

bool ble_gamepad_connected(void) { return fake_ble_started && (fake_ble_connected ? fake_ble_connected() : true); }
uint8_t ble_gamepad_raw(void) { return fake_ble_raw; }
uint32_t ble_gamepad_reports(void) { return 0; }
