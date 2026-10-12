#include "power.h"
#include "game_audio.h"
#include "game_session.h"
#include "game_storage.h"
#include "menu.h"
#include "screen.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "player";

void power_off(void)
{
    if (!game_session_ensure_saved()) return;
    esp_err_t error = esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
    if (!error) error = rtc_gpio_pullup_en(GPIO_NUM_0);
    if (!error) error = rtc_gpio_pulldown_dis(GPIO_NUM_0);
    if (!error) error = game_storage_close();
    if (error) { menu_notice("POWER ERROR", "COULD NOT PREPARE SLEEP", "TRY AGAIN"); return; }
    if (!screen_message("OFF", "GAME PROGRESS SAVED", "PRESS BOOT TO TURN ON", "SAFE TO REMOVE SD CARD")) return;
    /* Panel supply is active-low. Keep it off while digital GPIOs sleep. */
    error = gpio_hold_en(GPIO_NUM_6);
    if (!error) error = gpio_hold_en(GPIO_NUM_17); /* Keep the battery latch on for BOOT wake. */
    if (error) {
        gpio_hold_dis(GPIO_NUM_6); gpio_hold_dis(GPIO_NUM_17);
        menu_notice("POWER ERROR", "COULD NOT HOLD POWER STATE", "TRY AGAIN"); return;
    }
    gpio_deep_sleep_hold_en();
    while (gpio_get_level(GPIO_NUM_0) == 0) vTaskDelay(pdMS_TO_TICKS(20));
    game_audio_shutdown();
    ESP_LOGI(TAG, "PLAYER_OFF,saved,1,wake,BOOT");
    esp_deep_sleep_start();
}

/* GPIO 17 latches the battery on when high. Preload HIGH before releasing a retained sleep hold
 * or enabling the output. */
esp_err_t power_enable_battery(void)
{
    esp_err_t error = gpio_set_level(GPIO_NUM_17, 1);
    const gpio_config_t battery = {.pin_bit_mask = 1ULL << GPIO_NUM_17,
        .mode = GPIO_MODE_OUTPUT, .pull_up_en = GPIO_PULLUP_ENABLE};
    if (!error) error = gpio_config(&battery);
    if (!error) error = gpio_hold_dis(GPIO_NUM_17);
    if (!error) ESP_LOGI(TAG, "PLAYER_BATTERY_LATCH,gpio,17,level,1");
    return error;
}
