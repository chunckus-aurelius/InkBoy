#pragma once
#include <stdint.h>

typedef unsigned UBaseType_t;
typedef int BaseType_t;
typedef uint32_t TickType_t;
#define pdPASS 1
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)
#define portMAX_DELAY 0xffffffffu
#define pdFALSE 0
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mutex) ((void)(mutex))
#define portEXIT_CRITICAL(mutex) ((void)(mutex))
#define portENTER_CRITICAL_ISR(mutex) ((void)(mutex))
#define portEXIT_CRITICAL_ISR(mutex) ((void)(mutex))
#define portYIELD_FROM_ISR() ((void)0)
