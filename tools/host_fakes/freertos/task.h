#pragma once
#include "freertos/FreeRTOS.h"

typedef struct host_task *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

void vTaskDelay(TickType_t ticks);
UBaseType_t uxTaskPriorityGet(TaskHandle_t task);
void vTaskPrioritySet(TaskHandle_t task, UBaseType_t priority);
BaseType_t xTaskCreate(TaskFunction_t function, const char *name, uint32_t stack, void *argument,
                       UBaseType_t priority, TaskHandle_t *handle);
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t function, const char *name, uint32_t stack,
                                   void *argument, UBaseType_t priority, TaskHandle_t *handle, int core);
void vTaskDelete(TaskHandle_t task);
BaseType_t xTaskNotifyGive(TaskHandle_t task);
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t wait);
