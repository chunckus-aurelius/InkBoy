#include "input.h"
#include "diagnostics.h"
#include "controller.h"
#include "gb_emulator.h"
#include "screen.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <string.h>

static const char *TAG = "player";
/* Indexed by the Game Boy button bit: A,B,Select,Start,Right,Left,Up,Down. */
static uint8_t mapping[8];
static nvs_handle_t settings;
static int read_pin(int pin)
{
#ifdef INKBOY_DIAGNOSTICS
    int simulated = diagnostics_gpio(pin);
    if (simulated >= 0) return simulated;
#endif
    return gpio_get_level(pin);
}

uint8_t input_buttons(void)
{
    uint8_t raw = controller_raw(), result = 0;
    for (unsigned i = 0; i < 8; ++i) if (raw & mapping[i]) result |= 1u << i;
    return result;
}

typedef struct { bool raw, stable, handled; int64_t changed, pressed; } button_state_t;
static button_state_t board_buttons[2];
static _Atomic bool gameplay;
static bool chord, chord_reported;
static int64_t chord_started;
static unsigned events[32];
static _Atomic unsigned event_read, event_write;

static void enqueue(unsigned event)
{
    unsigned write = atomic_load(&event_write), read = atomic_load(&event_read);
    if (write - read < 32) { events[write % 32] = event;
#ifdef INKBOY_DIAGNOSTICS
        ESP_LOGI("test", "PLAYER_BUTTON,event,%u,game,%u",event,atomic_load(&gameplay));
#endif
         atomic_store(&event_write, write + 1); }
}

unsigned input_hardware_event(void)
{
    unsigned read = atomic_load(&event_read);
    if (read == atomic_load(&event_write)) return INPUT_NONE;
    unsigned event = events[read % 32];
    atomic_store(&event_read, read + 1);
    if (event == INPUT_PC) (void)controller_toggle_usb();
    return event;
}

void input_set_gameplay(bool enabled) { atomic_store(&gameplay, enabled); }
void input_clear_events(void) { while (input_hardware_event()) {} }

void input_sample_hardware(void)
{
    int64_t now = esp_timer_get_time();
    bool down[2] = {read_pin(GPIO_NUM_0) == 0, read_pin(GPIO_NUM_18) == 0};
    if (down[0] && down[1]) {
        if (!chord) {
            if (atomic_load(&gameplay)) enqueue(INPUT_BACK);
            chord=true;chord_reported=false;
        }
        if (!chord_started) chord_started=now;
        if (!chord_reported && now-chord_started>=2000000) {
            enqueue(INPUT_PC);chord_reported=true;
        }
    } else chord_started=0;
    for (unsigned i = 0; i < 2; ++i) {
        button_state_t *b = &board_buttons[i];
        bool raw = down[i];
        if (raw != b->raw) { b->raw = raw; b->changed = now; }
        if (now - b->changed >= 30000 && b->stable != raw) {
            b->stable = raw;
            if (raw) { b->pressed = now; b->handled = chord; }
            else if (!b->handled) { enqueue(i ? INPUT_NEXT : INPUT_SELECT); b->handled = true; }
        }
        if (chord) b->handled = true;
        if (!b->stable || b->handled) continue;
        if (atomic_load(&gameplay)) {
            enqueue(i ? INPUT_NEXT : INPUT_SELECT); b->handled = true;
        } else if (now - b->pressed >= 700000) {
            enqueue(i ? INPUT_BACK : INPUT_PREVIOUS); b->handled = true;
        }
    }
    if (!down[0] && !down[1] && !board_buttons[0].stable && !board_buttons[1].stable) chord = false;
}

static void watch_buttons(void *unused)
{
    (void)unused;
    for (;;) { input_sample_hardware(); vTaskDelay(pdMS_TO_TICKS(10)); }
}

esp_err_t input_start_hardware_watch(void)
{
    const gpio_config_t buttons = {.pin_bit_mask = (1ULL << GPIO_NUM_0) | (1ULL << GPIO_NUM_18),
        .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE};
    esp_err_t error = gpio_config(&buttons);
    if (error) return error;
    for (unsigned i = 0; i < 2; ++i) {
        bool down = read_pin(i ? GPIO_NUM_18 : GPIO_NUM_0) == 0;
        board_buttons[i] = (button_state_t){.raw = down, .stable = down, .handled = true};
    }
#ifdef INKBOY_DIAGNOSTICS
    diagnostics_start();
#endif
    return xTaskCreate(watch_buttons, "player_buttons", 2048, NULL, 2, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

bool input_connected(void) { return controller_connected(); }
uint32_t input_report_count(void) { return controller_reports(); }

bool input_wait_controller(void)
{
    if (controller_connected()) return true;
    const bool usb = controller_transport() == CONTROLLER_USB;
    if (!screen_message("ON / CONNECT", usb ? "PLUG IN THE USB CONTROLLER" : "TURN ON ESP32 GAMEPAD",
                        usb ? "CONNECTING OVER USB" : "CONNECTING AUTOMATICALLY", "HOLD PWR TO RETURN")) return false;
    while (!controller_connected()) { if ((input_hardware_event() >= INPUT_BACK)) return false;
        (void)screen_service(); vTaskDelay(pdMS_TO_TICKS(20)); }
    return true;
}

void input_wait_release(void)
{
    while (controller_raw() || read_pin(GPIO_NUM_0) == 0 || read_pin(GPIO_NUM_18) == 0) vTaskDelay(pdMS_TO_TICKS(20));
    vTaskDelay(pdMS_TO_TICKS(40));
    input_clear_events();
}

bool input_mapping_valid(void)
{
    uint8_t used = 0;
    for (unsigned i=0;i<8;++i) {
        uint8_t b = mapping[i];
        if (!b || (b & (b-1)) || (used & b)) return false;
        used |= b;
    }
    return used == 255;
}

static bool mapping_release(void)
{
    while (controller_raw() || read_pin(GPIO_NUM_0) == 0 || read_pin(GPIO_NUM_18) == 0) {
        if (!controller_connected() || (input_hardware_event() >= INPUT_BACK)) return false;
        if (screen_service() < 0) return false;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return true;
}

bool input_calibrate(void)
{
    static const char *names[] = {"PRESS UP", "PRESS DOWN", "PRESS LEFT", "PRESS RIGHT",
                                  "PRESS A", "PRESS B", "PRESS START", "PRESS SELECT"};
    static const uint8_t order[] = {6,7,5,4,0,1,3,2};
    uint8_t used = 0;
    uint8_t candidate[8] = {0};
    for (unsigned step=0;step<8;++step) {
        if (!input_wait_controller()) return false;
        if (!mapping_release()) return false;
        if (!screen_prompt(names[step], "PRESS ONE CONTROLLER BUTTON")) return false;
        ESP_LOGI(TAG, "MAP_WAIT,step,%u,control,%s", step, names[step]);
        for (;;) {
            if (!controller_connected() || (input_hardware_event() >= INPUT_BACK)) return false;
            if (screen_service() < 0) return false;
            uint8_t raw = controller_raw();
            if (raw && !(raw & (raw-1)) && !(raw & used)) {
                candidate[order[step]] = raw;
                used |= raw;
                ESP_LOGI(TAG, "MAP_BUTTON,control,%s,raw,%02x", names[step], raw);
                if (!mapping_release()) return false;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    esp_err_t error = nvs_set_blob(settings, "buttons", candidate, sizeof(candidate));
    if (!error) error = nvs_commit(settings);
    if (error) {
        (void)nvs_set_blob(settings, "buttons", mapping, sizeof(mapping));
        (void)nvs_commit(settings);
        screen_message("SETUP ERROR", "CANNOT SAVE BUTTON MAPPING", "PREVIOUS MAPPING KEPT", "");
        return false;
    }
    memcpy(mapping, candidate, sizeof(mapping));
    ESP_LOGI(TAG, "MAP_SAVED");
    return true;
}

void input_remap(void)
{
    uint8_t old_mapping[8]; memcpy(old_mapping, mapping, sizeof(mapping));
    if (!input_calibrate()) memcpy(mapping, old_mapping, sizeof(mapping));
}

void input_load_mapping(nvs_handle_t nvs)
{
    settings = nvs;
    size_t map_size = sizeof(mapping);
    if (nvs_get_blob(settings, "buttons", mapping, &map_size) != ESP_OK || map_size != sizeof(mapping) || !input_mapping_valid())
        memset(mapping, 0, sizeof(mapping));
    if (!input_mapping_valid()) {
        /* Down=GPIO0, Left=1, Up=2, Right=3, Select=4, B=5, A=6, Start=7 on the
         * controller. A saved mapping wins and nothing is written here. */
        const uint8_t published_wiring[] = {0x40,0x20,0x10,0x80,0x08,0x02,0x04,0x01};
        memcpy(mapping, published_wiring, sizeof(mapping));
        ESP_LOGI(TAG, "PLAYER_MAPPING: using the published controller wiring");
    }
}
