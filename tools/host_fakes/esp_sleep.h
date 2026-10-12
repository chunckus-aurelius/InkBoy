#pragma once
#include "esp_err.h"

esp_err_t esp_sleep_enable_ext0_wakeup(int pin, int level);
void esp_deep_sleep_start(void);
