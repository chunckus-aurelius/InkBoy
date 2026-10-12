#pragma once
#include <stdint.h>
#include "esp_err.h"

#define GPIO_NUM_0 0
#define GPIO_NUM_6 6
#define GPIO_NUM_17 17
#define GPIO_NUM_18 18
#define GPIO_MODE_INPUT 1
#define GPIO_MODE_OUTPUT 2
#define GPIO_PULLUP_ENABLE 1
#define GPIO_INTR_ANYEDGE 3

typedef int gpio_num_t;
typedef struct { uint64_t pin_bit_mask; int mode, pull_up_en, pull_down_en, intr_type; } gpio_config_t;
typedef void (*gpio_isr_t)(void *);

esp_err_t gpio_config(const gpio_config_t *config);
esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level);
int gpio_get_level(gpio_num_t pin);
esp_err_t gpio_hold_en(gpio_num_t pin);
esp_err_t gpio_hold_dis(gpio_num_t pin);
void gpio_deep_sleep_hold_en(void);
void gpio_deep_sleep_hold_dis(void);
esp_err_t gpio_set_direction(gpio_num_t pin, int mode);
esp_err_t gpio_install_isr_service(int flags);
void gpio_uninstall_isr_service(void);
esp_err_t gpio_isr_handler_add(gpio_num_t pin, gpio_isr_t handler, void *argument);
esp_err_t gpio_isr_handler_remove(gpio_num_t pin);
