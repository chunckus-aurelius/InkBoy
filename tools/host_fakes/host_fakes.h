#pragma once
/* Test-side controls for the ESP-IDF fakes in this directory. The fakes are
 * deterministic: time only moves when code calls vTaskDelay or a test moves it. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

void host_fakes_reset(void);

/* Virtual clock behind esp_timer_get_time() and vTaskDelay(). Each non-zero
 * delay wakes host_delay_early_us before the requested whole-millisecond tick. */
extern int64_t host_clock_us;
extern int64_t host_delay_early_us;

/* FreeRTOS: tasks are recorded and only run when a test asks. host_task_run()
 * executes one until it blocks on an empty notification. A task named by
 * host_task_autorun_name also runs inside every xTaskNotifyGive() to it, which
 * models a worker that is scheduled as soon as it is woken. */
extern unsigned host_task_priority, host_tasks_created, host_mutexes_held;
extern bool host_task_create_fails;
extern bool host_mutex_create_fails;
bool host_mutex_is_locked(SemaphoreHandle_t mutex);
extern char host_task_autorun_name[16];
void host_task_run(TaskHandle_t task);

/* Heap: every allocation is real; the hook or flag can make it fail. */
extern bool host_heap_fail_all;
extern bool (*host_heap_fail_hook)(size_t size, unsigned caps);

/* Log lines are kept for assertions; set HOST_LOG=1 to echo them to stderr. */
void host_log_clear(void);
unsigned host_log_count(const char *needle);
const char *host_log_last(const char *needle);

/* Ordered record of hardware side effects, such as "gpio_hold_en(6)". */
void host_trace(const char *format, ...) __attribute__((format(printf, 1, 2)));
void host_trace_clear(void);
bool host_trace_has(const char *needle);
/* Position of needle in the trace, or -1. Used to assert ordering. */
int host_trace_position(const char *needle);

extern int host_gpio_level[40];
extern esp_err_t host_wake_enable_error;
extern void (*host_deep_sleep_hook)(void);
extern int host_reset_reason;

/* SPI: only the last transaction and a call count are kept. */
typedef struct { size_t length; const void *buffer; unsigned override_hz; } host_spi_transaction_t;
extern host_spi_transaction_t host_spi_last;
extern unsigned host_spi_transactions;
extern esp_err_t host_spi_error;

/* In-memory NVS. Writes are staged per handle and become visible to other
 * handles only on commit, like the real flash-backed store. */
typedef enum { HOST_NVS_U8 = 1, HOST_NVS_U16, HOST_NVS_BLOB, HOST_NVS_STR } host_nvs_type_t;
typedef struct { host_nvs_type_t type; size_t size; uint8_t data[64]; } host_nvs_entry_t;
extern esp_err_t host_nvs_open_error, host_nvs_get_error, host_nvs_set_error, host_nvs_commit_error;
extern unsigned host_nvs_commits;
void host_nvs_reset(void);
void host_nvs_seed(const char *space, const char *key, host_nvs_type_t type, const void *data, size_t size);
const host_nvs_entry_t *host_nvs_find(const char *space, const char *key);
uint8_t host_nvs_u8(const char *space, const char *key);
void host_nvs_erase(const char *space, const char *key);
void host_nvs_restore(const char *space, const char *key, const host_nvs_entry_t *entry);
