#include "fake_gray_backend.h"
#include <assert.h>

fake_gray_backend_t fake_gray;

void fake_gray_reset(void)
{
    fake_gray = (fake_gray_backend_t){.config = GRAY_CONFIG_DEFAULT, .adaptive = GRAY_ADAPTIVE_DEFAULT, .holds = GRAY_HOLD_ALL};
}

esp_err_t fast_display_set_gray_config(const gray_config_t *config)
{
    assert(gray_config_valid(config));
    fake_gray.config = *config;
    ++fake_gray.applies;
    return ESP_OK;
}

esp_err_t fast_display_reserve_gray_adaptive(const gray_adaptive_t *config)
{
    assert(gray_adaptive_valid(config));
    if (fake_gray.adaptive_alloc_error && ((config->history_mode && !fake_gray.history_memory) ||
                                           (config->weak_wl && !fake_gray.weak_memory)))
        return ESP_ERR_NO_MEM;
    if (config->history_mode) fake_gray.history_memory = true;
    if (config->weak_wl) fake_gray.weak_memory = true;
    return ESP_OK;
}

esp_err_t fast_display_set_gray_adaptive(const gray_adaptive_t *config)
{
    esp_err_t error = fast_display_reserve_gray_adaptive(config);
    if (error) return error;
    fake_gray.adaptive = *config;
    if (!config->history_mode) fake_gray.history_memory = false;
    if (!config->weak_wl) fake_gray.weak_memory = false;
    ++fake_gray.applies;
    return ESP_OK;
}

esp_err_t fast_display_set_gray_strength(unsigned preset)
{
    assert(preset < 4);
    fake_gray.strength = preset;
    ++fake_gray.applies;
    return ESP_OK;
}

esp_err_t fast_display_set_gray_immediate(unsigned enabled)
{
    fake_gray.immediate = enabled != 0;
    ++fake_gray.applies;
    return ESP_OK;
}

esp_err_t fast_display_set_gray_holds(unsigned mask)
{
    assert(gray_hold_valid(mask));
    fake_gray.holds = mask;
    ++fake_gray.applies;
    return ESP_OK;
}

void fast_display_set_gray_game(const uint8_t *packed) { (void)packed; }
