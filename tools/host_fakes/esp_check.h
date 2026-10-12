#pragma once
#include "esp_err.h"

#define ESP_RETURN_ON_ERROR(expression, tag, ...) do { esp_err_t host_check = (expression); if (host_check != ESP_OK) return host_check; } while (0)
