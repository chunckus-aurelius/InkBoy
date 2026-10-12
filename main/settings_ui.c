#include "settings_ui.h"
#include "controller.h"
#include "display_config.h"
#include "display_preset.h"
#include "fast_display.h"
#include "game_audio.h"
#include "gray_settings_ui.h"
#include "input.h"
#include "menu.h"
#include "preset_ui.h"
#include "screen.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "player";
static nvs_handle_t settings;
static bool sound_enabled = true;

void settings_ui_init(nvs_handle_t nvs) { settings = nvs; }
bool settings_ui_sound_enabled(void) { return sound_enabled; }

void settings_ui_load_display(void)
{
    display_config_t config = display_config_get();
    uint8_t stored_mode;
    if (nvs_get_u8(settings, "display_mode", &stored_mode) == ESP_OK && stored_mode < DISPLAY_MODE_COUNT)
        config.mode = (display_mode_t)stored_mode;
    const display_preset_t *dithered = display_preset_active(DISPLAY_DITHERED);
    config.dither_black_pulses = dithered->black_pulses;
    config.dither_white_pulses = dithered->white_pulses;
    display_config_set(&config);
    ESP_LOGI(TAG, "PLAYER_DITHER_SETTINGS,black,%u,white,%u", config.dither_black_pulses, config.dither_white_pulses);
    ESP_LOGI(TAG, "PLAYER_SETTINGS,mode,%u", config.mode);
}

void settings_ui_load_toggles(void)
{
    display_config_t config = display_config_get();
    uint8_t stored_fps=0;
    if (nvs_get_u8(settings,"show_fps",&stored_fps)==ESP_OK && stored_fps<=1)
        config.show_fps=stored_fps;
    display_config_set(&config);
    ESP_LOGI(TAG,"PLAYER_FPS,enabled,%u",config.show_fps);
    uint8_t stored_sound = 1;
    if (nvs_get_u8(settings, "sound", &stored_sound) == ESP_OK && stored_sound <= 1)
        sound_enabled = stored_sound;
}


static esp_err_t save_value(const char *key, uint8_t next, uint8_t previous)
{
    esp_err_t error = nvs_set_u8(settings, key, next);
    if (error) return error;
    error = nvs_commit(settings);
    if (error) {
        /* Prevent a later successful commit from applying this failed edit. */
        (void)nvs_set_u8(settings, key, previous);
        (void)nvs_commit(settings);
    }
    return error;
}

static void change_mode(void)
{
    const char *items[] = {"FAST DITHERED", "FAST GRAYSCALE"};
    display_config_t next = display_config_get(); unsigned selected = next.mode;
    int choice = menu_choose("DISPLAY MODE", "", items, 2, &selected, NULL);
    if (choice < 0 || choice == (int)next.mode) return;
    display_mode_t previous = next.mode;
    next.mode = (display_mode_t)choice;
    esp_err_t error = save_value("display_mode", next.mode, previous);
    if (error) menu_notice("SETTINGS ERROR", "COULD NOT SAVE DISPLAY MODE", "PREVIOUS VALUE KEPT");
    else { display_config_set(&next); ESP_LOGI(TAG, "PLAYER_SETTINGS,mode,%u", next.mode); }
}

static void dither_settings(void)
{
    unsigned selected = 0;
    while (!screen_failed()) {
        const display_config_t config = display_config_get(); char black[32], white[32];
        snprintf(black, sizeof black, "BLACK PULSES: %u", config.dither_black_pulses);
        snprintf(white, sizeof white, "WHITE PULSES: %u", config.dither_white_pulses);
        const char *items[] = {black, white}; int direction = 1;
        int choice = menu_choose("DITHERED TUNING", "", items, 2, &selected, &direction);
        if (choice < 0) return;
        display_preset_t next = *display_preset_active(DISPLAY_DITHERED);
        uint8_t *value = choice == 0 ? &next.black_pulses : &next.white_pulses;
        *value = (uint8_t)(1 + ((*value - 1 + (direction > 0 ? 1 : DITHER_PULSES_MAX - 1)) % DITHER_PULSES_MAX));
        if (preset_ui_activate(&next, true)) menu_notice("SETTINGS ERROR", "COULD NOT SAVE PULSES", "PREVIOUS VALUE KEPT");
        else ESP_LOGI(TAG, "PLAYER_DITHER_SETTINGS,black,%u,white,%u", next.black_pulses, next.white_pulses);
    }
}

static void display_settings(void)
{
    unsigned selected = 0;
    while (!screen_failed()) {
        const display_config_t config = display_config_get(); char mode[40], preset[40], active[PRESET_NAME_MAX + 3];
        snprintf(mode, sizeof mode, "MODE: %s", display_config_mode_name());
        display_preset_active_label(config.mode, active, sizeof active);
        snprintf(preset, sizeof preset, "PRESET: %s", active);
        const char *items[] = {mode, preset, "TUNING", "TRANSITION TEST"};
        int choice = menu_choose("DISPLAY", "", items, config.mode == DISPLAY_GRAYSCALE ? 4 : 3, &selected, NULL);
        if (choice < 0) return;
        if (choice == 0) change_mode();
        else if (choice == 1) preset_ui_open();
        else if (choice == 2) { if (config.mode == DISPLAY_DITHERED) dither_settings(); else gray_settings_ui_open(); }
        else gray_settings_ui_transition_test();
    }
}

static void controller_settings(void)
{
    unsigned selected = 0;
    while (!screen_failed()) {
        char connection[40];
        snprintf(connection, sizeof connection, "CONNECTION: %s", controller_transport() == CONTROLLER_USB ? "USB" : "BLUETOOTH");
        const char *items[] = {connection, "MAP BUTTONS"};
        int choice = menu_choose("CONTROLLER", "", items, 2, &selected, NULL);
        if (choice < 0) return;
        if (choice == 1) { input_remap(); continue; }
        const char *connections[] = {"BLUETOOTH", "USB"};
        unsigned transport = controller_transport();
        choice = menu_choose("CONNECTION", "", connections, 2, &transport, NULL);
        if (choice < 0 || (choice == (int)controller_transport() && controller_running())) continue;
        if (choice == CONTROLLER_USB && !menu_confirm("USB CONTROLLER", "HOLD BOTH BUTTONS FOR SERIAL", "USE USB")) continue;
        if (!screen_close()) return;
        esp_err_t error = controller_select((controller_transport_t)choice);
        if (error) menu_notice("CONTROLLER ERROR", "COULD NOT SWITCH CONTROLLER", controller_running() ? "PREVIOUS CONTROLLER KEPT" : "CHOOSE ONE AGAIN");
    }
}

static void toggle_settings(bool sound)
{
    unsigned selected = 0;
    while (!screen_failed()) {
        display_config_t config = display_config_get(); char label[32];
        bool value = sound ? sound_enabled : config.show_fps;
        snprintf(label, sizeof label, "%s: %s", sound ? "GAME SOUND" : "SHOW FPS", value ? "ON" : "OFF");
        const char *items[] = {label};
        if (menu_choose(sound ? "SOUND" : "FPS", "", items, 1, &selected, NULL) < 0) return;
        esp_err_t error = save_value(sound ? "sound" : "show_fps", !value, value);
        if (error) menu_notice("SETTINGS ERROR", "COULD NOT SAVE SETTING", "PREVIOUS VALUE KEPT");
        else if (sound) { sound_enabled = !value; game_audio_set_enabled(sound_enabled); }
        else { config.show_fps = !value; display_config_set(&config); }
    }
}

void settings_ui_open(void)
{
    unsigned selected = 0;
    const char *items[] = {"DISPLAY", "CONTROLLER", "SOUND", "FPS"};
    while (!screen_failed()) {
        int choice = menu_choose("SETTINGS", "", items, 4, &selected, NULL);
        if (choice < 0) return;
        if (choice == 0) display_settings();
        else if (choice == 1) controller_settings();
        else toggle_settings(choice == 2);
    }
}
