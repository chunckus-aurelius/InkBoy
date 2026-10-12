#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "host_fakes.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static char trace[4096];
int host_gpio_level[40] = {[0] = 1};
esp_err_t host_wake_enable_error;
void (*host_deep_sleep_hook)(void);
int host_reset_reason;

void host_trace(const char *format, ...)
{
    size_t used = strlen(trace);
    if (used + 160 > sizeof(trace)) return;
    va_list args;
    va_start(args, format);
    vsnprintf(trace + used, sizeof(trace) - used - 2, format, args);
    va_end(args);
    strcat(trace, ";");
}

void host_trace_clear(void) { trace[0] = 0; }
bool host_trace_has(const char *needle) { return strstr(trace, needle) != NULL; }

int host_trace_position(const char *needle)
{
    const char *found = strstr(trace, needle);
    return found ? (int)(found - trace) : -1;
}

esp_err_t gpio_config(const gpio_config_t *config)
{
    host_trace("gpio_config(mask=%llx,mode=%d,pullup=%d)", (unsigned long long)config->pin_bit_mask, config->mode, config->pull_up_en);
    return ESP_OK;
}

esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level)
{
    host_trace("gpio_set_level(%d,%u)", pin, level);
    return ESP_OK;
}

int gpio_get_level(gpio_num_t pin) { return host_gpio_level[pin]; }

esp_err_t gpio_hold_en(gpio_num_t pin)
{
    host_trace("gpio_hold_en(%d)", pin);
    return ESP_OK;
}

esp_err_t gpio_hold_dis(gpio_num_t pin)
{
    host_trace("gpio_hold_dis(%d)", pin);
    return ESP_OK;
}

void gpio_deep_sleep_hold_en(void) { host_trace("gpio_deep_sleep_hold_en"); }
void gpio_deep_sleep_hold_dis(void) { host_trace("gpio_deep_sleep_hold_dis"); }

esp_err_t rtc_gpio_pullup_en(gpio_num_t pin)
{
    host_trace("rtc_gpio_pullup_en(%d)", pin);
    return ESP_OK;
}

esp_err_t rtc_gpio_pulldown_dis(gpio_num_t pin)
{
    host_trace("rtc_gpio_pulldown_dis(%d)", pin);
    return ESP_OK;
}

esp_err_t rtc_gpio_deinit(gpio_num_t pin)
{
    host_trace("rtc_gpio_deinit(%d)", pin);
    return ESP_OK;
}

esp_err_t esp_sleep_enable_ext0_wakeup(int pin, int level)
{
    host_trace("esp_sleep_enable_ext0_wakeup(%d,%d)", pin, level);
    return host_wake_enable_error;
}

void esp_deep_sleep_start(void)
{
    host_trace("esp_deep_sleep_start");
    if (host_deep_sleep_hook) host_deep_sleep_hook();
}

esp_reset_reason_t esp_reset_reason(void) { return (esp_reset_reason_t)host_reset_reason; }
