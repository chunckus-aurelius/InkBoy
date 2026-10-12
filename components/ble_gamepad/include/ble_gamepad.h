#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
/* Start and stop are idempotent. Stop ends scanning and releases the
 * controller, returning ESP_ERR_TIMEOUT if either is still in progress. */
esp_err_t ble_gamepad_start(void);
esp_err_t ble_gamepad_stop(void);
bool ble_gamepad_connected(void);
uint8_t ble_gamepad_raw(void);
uint32_t ble_gamepad_reports(void);
