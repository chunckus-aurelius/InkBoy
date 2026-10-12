#include "controller.h"
#include "diagnostics.h"
#include "ble_gamepad.h"
#include "gameboy/usb_gamepad.h"
#include "esp_log.h"
#include <stdarg.h>
#include <stdatomic.h>
#include <stddef.h>

static const char *TAG = "player";
enum { USB_STOP_ATTEMPTS = 3 };

static nvs_handle_t settings;
static controller_transport_t saved, active;
static bool running, console_mode;
static vprintf_like_t previous_log;
static _Atomic uint8_t usb_mask;
static _Atomic bool usb_report_seen;
static _Atomic uint32_t usb_reports;

const char *controller_transport_name(controller_transport_t transport)
{
    return transport == CONTROLLER_USB ? "usb" : "ble";
}

static void usb_report(uint8_t mask, void *unused)
{
    (void)unused;
    atomic_store(&usb_mask, mask);
    atomic_store(&usb_report_seen, true);
    atomic_fetch_add(&usb_reports, 1);
}

static void usb_disconnected(void *unused)
{
    (void)unused;
    atomic_store(&usb_mask, 0);
    atomic_store(&usb_report_seen, false);
}

static int discard_log(const char *format, va_list args)
{
    (void)format; (void)args;
    return 0;
}

static void release_console(void *unused)
{
    (void)unused;
    if (!previous_log) previous_log = esp_log_set_vprintf(discard_log);
}

static void restore_console(void *unused)
{
    (void)unused;
    if (!previous_log) return;
    esp_log_set_vprintf(previous_log);
    previous_log = NULL;
}

static esp_err_t start_transport(controller_transport_t transport)
{
    if (transport == CONTROLLER_BLUETOOTH) return ble_gamepad_start();
    if (console_mode) return ESP_OK;
    const usb_gamepad_config_t config = {
        .report = usb_report, .disconnected = usb_disconnected,
        .release_console = release_console, .restore_console = restore_console,
    };
    return usb_gamepad_start(&config);
}

static esp_err_t stop_transport(controller_transport_t transport)
{
    if (transport == CONTROLLER_BLUETOOTH) return ble_gamepad_stop();
    esp_err_t error = ESP_ERR_TIMEOUT;
    for (unsigned attempt = 0; attempt < USB_STOP_ATTEMPTS && error == ESP_ERR_TIMEOUT; ++attempt)
        error = usb_gamepad_stop();
    if (error == ESP_OK) usb_disconnected(NULL);
    return error;
}

static esp_err_t start_and_log(controller_transport_t transport)
{
    esp_err_t error = start_transport(transport);
    running = error == ESP_OK;
    if (running) active = transport;
    ESP_LOGI(TAG, "PLAYER_CONTROLLER,transport,%s,result,%s", controller_transport_name(transport), esp_err_to_name(error));
    return error;
}

static esp_err_t save(controller_transport_t transport)
{
    esp_err_t error = nvs_set_u8(settings, "controller", (uint8_t)transport);
    if (!error) error = nvs_commit(settings);
    if (!error) saved = transport;
    return error;
}

void controller_load(nvs_handle_t nvs)
{
    settings = nvs;
    console_mode = false;
    uint8_t stored;
    saved = nvs_get_u8(settings, "controller", &stored) == ESP_OK && stored < CONTROLLER_TRANSPORT_COUNT
                ? (controller_transport_t)stored : CONTROLLER_BLUETOOTH;
    active = saved;
}

controller_transport_t controller_transport(void) { return active; }
bool controller_running(void) { return running; }

esp_err_t controller_start(void)
{
    esp_err_t error = start_and_log(saved);
    if (error && saved == CONTROLLER_USB) error = start_and_log(CONTROLLER_BLUETOOTH);
    return error;
}

esp_err_t controller_select(controller_transport_t transport)
{
    const controller_transport_t previous = active;
    if (transport == previous && running) return transport == saved ? ESP_OK : save(transport);
    esp_err_t error = running ? stop_transport(previous) : ESP_OK;
    if (error) {
        ESP_LOGW(TAG, "PLAYER_CONTROLLER,transport,%s,result,%s", controller_transport_name(transport), esp_err_to_name(error));
        return error;
    }
    running = false;
    error = save(transport);
    if (!error) { console_mode = false; error = start_and_log(transport); }
    if (error) {
        (void)save(previous);
        (void)start_and_log(previous);
    }
    return error;
}

/* Startup bypass only: retain serial without changing the saved USB preference. */
void controller_request_console(void) { console_mode = true; }

esp_err_t controller_toggle_usb(void)
{
    if (active != CONTROLLER_USB) return ESP_OK;
    esp_err_t error;
    if (!console_mode) {
        error = stop_transport(CONTROLLER_USB);
        if (!error) console_mode = true;
    } else {
        console_mode = false;
        error = start_transport(CONTROLLER_USB);
        if (error) console_mode = !usb_gamepad_running();
    }
    ESP_LOGI(TAG, "PLAYER_USB_ACCESS,serial,%u,result,%s", console_mode, esp_err_to_name(error));
    return error;
}

bool controller_connected(void)
{
#ifdef INKBOY_DIAGNOSTICS
    if (diagnostics_controller() >= 0) return true;
#endif
    if (!running || (active == CONTROLLER_USB && console_mode)) return false;
    return active == CONTROLLER_USB ? usb_gamepad_running() && atomic_load(&usb_report_seen) : ble_gamepad_connected();
}

uint8_t controller_raw(void)
{
#ifdef INKBOY_DIAGNOSTICS
    int simulated = diagnostics_controller();
    if (simulated >= 0) return (uint8_t)simulated;
#endif
    if (!controller_connected()) return 0;
    return active == CONTROLLER_USB ? atomic_load(&usb_mask) : ble_gamepad_raw();
}

uint32_t controller_reports(void)
{
    return active == CONTROLLER_USB ? atomic_load(&usb_reports) : ble_gamepad_reports();
}

controller_status_t controller_status(void)
{
    if (active == CONTROLLER_USB && console_mode) return CONTROLLER_USB_PC;
    unsigned base = active == CONTROLLER_USB ? CONTROLLER_USB_OFF : CONTROLLER_BLE_OFF;
    return (controller_status_t)(base + (!running ? 0 : controller_connected() ? 2 : 1));
}
