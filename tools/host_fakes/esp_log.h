#pragma once
#include <stdarg.h>
#include "esp_err.h"

void host_log(char level, const char *tag, const char *format, ...) __attribute__((format(printf, 3, 4)));

#define ESP_LOGE(tag, ...) host_log('E', tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) host_log('W', tag, __VA_ARGS__)
#define ESP_LOGI(tag, ...) host_log('I', tag, __VA_ARGS__)
#define ESP_LOGD(tag, ...) host_log('D', tag, __VA_ARGS__)

typedef int (*vprintf_like_t)(const char *format, va_list args);
vprintf_like_t esp_log_set_vprintf(vprintf_like_t func);
