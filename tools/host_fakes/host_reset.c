#include "host_fakes.h"
#include <string.h>

void host_fakes_reset(void)
{
    host_clock_us = 1;
    host_delay_early_us = 0;
    host_task_priority = 1;
    host_tasks_created = host_mutexes_held = 0;
    host_task_autorun_name[0] = 0;
    host_task_create_fails = host_mutex_create_fails = false;
    host_heap_fail_all = false;
    host_heap_fail_hook = NULL;
    host_log_clear();
    host_trace_clear();
    memset(host_gpio_level, 0, sizeof(host_gpio_level));
    host_gpio_level[0] = 1; host_gpio_level[18] = 1;
    host_wake_enable_error = ESP_OK;
    host_deep_sleep_hook = NULL;
    host_reset_reason = 0;
    host_spi_transactions = 0;
    host_spi_error = ESP_OK;
    host_nvs_reset();
}
