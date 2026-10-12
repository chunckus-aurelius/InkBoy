#include "preset_ui.h"
#include "display_config.h"
#include "gray_settings_ui.h"
#include "menu.h"
#include "preset_store.h"
#include "screen.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "player";
static preset_entry_t entries[PRESET_LIST_MAX];

esp_err_t preset_ui_activate(const display_preset_t *preset, bool modified)
{
    if (preset->mode == DISPLAY_GRAYSCALE) return gray_settings_ui_activate(preset, modified);
    esp_err_t error = display_preset_set_active(preset, modified);
    if (error) return error;
    display_config_t config = display_config_get();
    config.dither_black_pulses = preset->black_pulses;
    config.dither_white_pulses = preset->white_pulses;
    display_config_set(&config);
    return ESP_OK;
}

static void report_store_error(preset_store_status_t status, const char *file, bool saving)
{
    switch (status) {
    case PRESET_STORE_NO_CARD: menu_notice("NO SD CARD", "INSERT A CARD FOR PRESETS", "NOTHING WAS CHANGED"); break;
    case PRESET_STORE_FULL: menu_notice("PRESETS FULL", "16 PRESETS MAX PER MODE", "DELETE A FILE FROM THE CARD"); break;
    case PRESET_STORE_NOT_FOUND: menu_notice("FILE MISSING", "PRESET FILE NOT ON THE CARD", "USE SAVE AS NEW"); break;
    case PRESET_STORE_INVALID: menu_notice("INVALID PRESET", "FILE CANNOT BE USED", file ? file : "CHECK THE JSON FILE"); break;
    default:
        if (saving) menu_notice("SAVE FAILED", "COULD NOT WRITE TO THE CARD", "PRESET WAS NOT SAVED");
        else menu_notice("SD CARD ERROR", "COULD NOT READ THE CARD", "NOTHING WAS CHANGED");
        break;
    }
}

static bool activate_or_report(const display_preset_t *preset, bool modified)
{
    esp_err_t error = preset_ui_activate(preset, modified);
    if (error) menu_notice("PRESET ERROR", "COULD NOT APPLY PRESET", "PREVIOUS SETTINGS KEPT");
    else ESP_LOGI(TAG, "PLAYER_PRESET,mode,%u,name,%s,modified,%u", preset->mode, preset->name, modified);
    return !error;
}

static bool confirm(const char *title, const char *question, const char *action)
{
    return menu_confirm(title, question, action);
}

static void load_preset(display_mode_t mode, char *status, size_t size)
{
    unsigned count = 0;
    preset_store_status_t found = preset_store_list(mode, entries, &count);
    if (found != PRESET_STORE_OK) {
        report_store_error(found, NULL, false);
        count = 0;
    }
    const char *items[PRESET_LIST_MAX + 2] = {"DEFAULT"};
    for (unsigned i = 0; i < count; ++i) items[i + 1] = entries[i].name;
    items[count + 1] = MENU_BACK;
    char subtitle[33];
    snprintf(subtitle, sizeof(subtitle), "%u SAVED PRESETS ON CARD", count);
    unsigned selected = 0;
    int choice = menu_choose("LOAD PRESET", subtitle, items, count + 2, &selected, NULL);
    if (choice < 0 || (unsigned)choice == count + 1) return;
    display_preset_t preset = *display_preset_default(mode);
    if (choice > 0) {
        preset_store_status_t loaded = preset_store_load(mode, entries[choice - 1].file, &preset);
        if (loaded != PRESET_STORE_OK) {
            report_store_error(loaded, entries[choice - 1].file, false);
            return;
        }
    }
    if (activate_or_report(&preset, false)) snprintf(status, size, "LOADED %s", preset.name);
}

static void save_as_new(display_mode_t mode, char *status, size_t size)
{
    display_preset_t saved;
    preset_store_status_t written = preset_store_save_new(display_preset_active(mode), &saved);
    if (written != PRESET_STORE_OK) {
        report_store_error(written, NULL, true);
        return;
    }
    if (activate_or_report(&saved, false)) snprintf(status, size, "SAVED AS %s", saved.name);
}

static void save_over_active(display_mode_t mode, char *status, size_t size)
{
    display_preset_t preset = *display_preset_active(mode);
    char question[33];
    snprintf(question, sizeof(question), "OVERWRITE %s?", preset.name);
    if (!confirm("SAVE PRESET", question, "OVERWRITE")) return;
    preset_store_status_t written = preset_store_overwrite(&preset);
    if (written != PRESET_STORE_OK) {
        report_store_error(written, NULL, true);
        return;
    }
    if (activate_or_report(&preset, false)) snprintf(status, size, "SAVED %s", preset.name);
}

static void restore_default(display_mode_t mode, char *status, size_t size)
{
    if (!confirm("RESTORE DEFAULT", "REPLACE CURRENT SETTINGS?", "RESTORE DEFAULT")) return;
    if (activate_or_report(display_preset_default(mode), false)) snprintf(status, size, "RESTORED DEFAULT");
}

typedef enum { PRESET_LOAD, PRESET_SAVE_NEW, PRESET_SAVE, PRESET_RESTORE, PRESET_BACK } preset_action_t;

void preset_ui_open(void)
{
    unsigned selected = 0;
    char status[33] = "";
    while (!screen_failed()) {
        const display_mode_t mode = display_config_get().mode;
        char label[PRESET_NAME_MAX + 3], subtitle[33];
        display_preset_active_label(mode, label, sizeof(label));
        snprintf(subtitle, sizeof(subtitle), "%s%s", status[0] ? "" : "ACTIVE: ", status[0] ? status : label);
        const char *items[PRESET_BACK + 1];
        preset_action_t actions[PRESET_BACK + 1];
        unsigned count = 0;
        items[count] = "LOAD PRESET"; actions[count++] = PRESET_LOAD;
        items[count] = "SAVE AS NEW"; actions[count++] = PRESET_SAVE_NEW;
        if (display_preset_active_is_user(mode) && display_preset_active_modified(mode)) {
            items[count] = "SAVE"; actions[count++] = PRESET_SAVE;
        }
        items[count] = "RESTORE DEFAULT"; actions[count++] = PRESET_RESTORE;
        items[count] = MENU_BACK; actions[count++] = PRESET_BACK;
        int choice = menu_choose("PRESET", subtitle, items, count, &selected, NULL);
        if (choice < 0 || actions[choice] == PRESET_BACK) return;
        status[0] = 0;
        switch (actions[choice]) {
        case PRESET_LOAD: load_preset(mode, status, sizeof(status)); break;
        case PRESET_SAVE_NEW: save_as_new(mode, status, sizeof(status)); break;
        case PRESET_SAVE: save_over_active(mode, status, sizeof(status)); break;
        default: restore_default(mode, status, sizeof(status)); break;
        }
    }
}
