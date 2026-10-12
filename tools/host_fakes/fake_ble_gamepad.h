#pragma once
#include "ble_gamepad.h"

extern bool (*fake_ble_connected)(void);
extern uint8_t fake_ble_raw;
extern esp_err_t fake_ble_start_error, fake_ble_stop_error;
extern unsigned fake_ble_starts, fake_ble_stops;
extern bool fake_ble_started;
