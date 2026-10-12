#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "nvs.h"

typedef enum { CONTROLLER_BLUETOOTH, CONTROLLER_USB, CONTROLLER_TRANSPORT_COUNT } controller_transport_t;

/* Loads the saved transport; an unknown stored value means Bluetooth. */
void controller_load(nvs_handle_t settings);
controller_transport_t controller_transport(void);
const char *controller_transport_name(controller_transport_t transport);

/* Starts the saved transport. A USB failure falls back to Bluetooth for this
 * boot without changing the saved choice. */
esp_err_t controller_start(void);

/* Switches transport and saves the choice. On failure the previous transport
 * is restarted; controller_running() reports whether that worked. */
esp_err_t controller_select(controller_transport_t transport);
bool controller_running(void);

bool controller_connected(void);
uint8_t controller_raw(void);
uint32_t controller_reports(void);

/* Startup serial bypass and runtime USB controller/serial toggle. Neither saves NVS. */
void controller_request_console(void);
esp_err_t controller_toggle_usb(void);

/* Snapshot used by menu badges; selected transport and live connection are separate. */
typedef enum { CONTROLLER_BLE_OFF, CONTROLLER_BLE_WAIT, CONTROLLER_BLE_CONNECTED,
               CONTROLLER_USB_OFF, CONTROLLER_USB_WAIT, CONTROLLER_USB_CONNECTED,
               CONTROLLER_USB_PC } controller_status_t;
controller_status_t controller_status(void);
