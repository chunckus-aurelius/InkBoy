#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "host_fakes.h"
#include <stdlib.h>

host_spi_transaction_t host_spi_last;
unsigned host_spi_transactions;
esp_err_t host_spi_error;

esp_err_t spi_bus_initialize(int host, const spi_bus_config_t *config, int dma) { (void)host; (void)config; (void)dma; return ESP_OK; }
esp_err_t spi_bus_free(int host) { (void)host; return ESP_OK; }

esp_err_t spi_bus_add_device(int host, const spi_device_interface_config_t *config, spi_device_handle_t *handle)
{
    (void)host; (void)config;
    *handle = (spi_device_handle_t)calloc(1, 1);
    return ESP_OK;
}

esp_err_t spi_bus_remove_device(spi_device_handle_t handle) { free(handle); return ESP_OK; }

esp_err_t spi_device_polling_transmit(spi_device_handle_t handle, spi_transaction_t *transaction)
{
    (void)handle;
    ++host_spi_transactions;
    host_spi_last = (host_spi_transaction_t){transaction->length, transaction->tx_buffer, transaction->override_freq_hz};
    return host_spi_error;
}

esp_err_t gpio_set_direction(gpio_num_t pin, int mode) { (void)pin; (void)mode; return ESP_OK; }
esp_err_t gpio_install_isr_service(int flags) { (void)flags; return ESP_OK; }
void gpio_uninstall_isr_service(void) {}
esp_err_t gpio_isr_handler_add(gpio_num_t pin, gpio_isr_t handler, void *argument) { (void)pin; (void)handler; (void)argument; return ESP_OK; }
esp_err_t gpio_isr_handler_remove(gpio_num_t pin) { (void)pin; return ESP_OK; }
