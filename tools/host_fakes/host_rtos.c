#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "host_fakes.h"
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int64_t host_clock_us = 1;
int64_t host_delay_early_us;
unsigned host_task_priority = 1, host_tasks_created, host_mutexes_held;
bool host_task_create_fails, host_mutex_create_fails;

struct host_task { TaskFunction_t function; void *argument; char name[16]; unsigned notifications; };
struct host_mutex { bool locked; };

char host_task_autorun_name[16];
static struct host_task *running;
static jmp_buf *blocked;

/* A task runs on the caller's stack until it blocks on an empty notification
 * or deletes itself; that is where control returns. */
void host_task_run(TaskHandle_t task)
{
    jmp_buf here;
    jmp_buf *outer_jump = blocked;
    struct host_task *outer = running;
    blocked = &here;
    running = task;
    if (!setjmp(here)) task->function(task->argument);
    running = outer;
    blocked = outer_jump;
}

int64_t esp_timer_get_time(void) { return host_clock_us; }

void vTaskDelay(TickType_t ticks)
{
    host_clock_us += ticks ? (int64_t)ticks * 1000 - host_delay_early_us : 0;
}

UBaseType_t uxTaskPriorityGet(TaskHandle_t task)
{
    assert(!task);
    return host_task_priority;
}

void vTaskPrioritySet(TaskHandle_t task, UBaseType_t priority)
{
    assert(!task);
    host_task_priority = priority;
}

BaseType_t xTaskCreate(TaskFunction_t function, const char *name, uint32_t stack, void *argument,
                       UBaseType_t priority, TaskHandle_t *handle)
{
    (void)stack; (void)priority;
    if (host_task_create_fails) return 0;
    ++host_tasks_created;
    struct host_task *task = calloc(1, sizeof(*task));
    task->function = function;
    task->argument = argument;
    snprintf(task->name, sizeof(task->name), "%s", name);
    if (handle) *handle = task;
    return pdPASS;
}

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t function, const char *name, uint32_t stack,
                                   void *argument, UBaseType_t priority, TaskHandle_t *handle, int core)
{
    (void)core;
    return xTaskCreate(function, name, stack, argument, priority, handle);
}

void vTaskDelete(TaskHandle_t task)
{
    assert(!task && running);
    longjmp(*blocked, 1);
}

BaseType_t xTaskNotifyGive(TaskHandle_t task)
{
    ++task->notifications;
    if (task != running && !strcmp(task->name, host_task_autorun_name)) host_task_run(task);
    return pdPASS;
}

uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t wait)
{
    (void)wait;
    assert(running);
    if (!running->notifications) longjmp(*blocked, 1);
    uint32_t taken = running->notifications;
    running->notifications = clear ? 0 : taken - 1;
    return taken;
}

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    return host_mutex_create_fails ? NULL : calloc(1, sizeof(struct host_mutex));
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t wait)
{
    (void)wait;
    assert(!mutex->locked);
    mutex->locked = true;
    ++host_mutexes_held;
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex)
{
    assert(mutex->locked);
    mutex->locked = false;
    --host_mutexes_held;
    return pdTRUE;
}

bool host_mutex_is_locked(SemaphoreHandle_t mutex) { return mutex->locked; }

SemaphoreHandle_t xSemaphoreCreateBinary(void) { return calloc(1, sizeof(struct host_mutex)); }
void vSemaphoreDelete(SemaphoreHandle_t semaphore) { free(semaphore); }

BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t semaphore, BaseType_t *woken)
{
    (void)semaphore;
    *woken = 0;
    return pdTRUE;
}
