#pragma once

#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_master_bus_handle_t handle;
} hal_i2c_bus_t;

typedef struct {
    i2c_master_dev_handle_t handle;
} hal_i2c_device_t;

esp_err_t hal_i2c_master_bus_init(hal_i2c_bus_t *bus,
                                  i2c_port_t port,
                                  gpio_num_t sda_gpio,
                                  gpio_num_t scl_gpio,
                                  uint32_t clk_hz);
void hal_i2c_master_bus_deinit(hal_i2c_bus_t *bus);

esp_err_t hal_i2c_master_device_add(hal_i2c_bus_t *bus,
                                    uint8_t addr,
                                    uint32_t clk_hz,
                                    hal_i2c_device_t *dev);
void hal_i2c_master_device_remove(hal_i2c_device_t *dev);

esp_err_t hal_i2c_master_device_write(hal_i2c_device_t *dev,
                                      const uint8_t *data,
                                      size_t len,
                                      uint32_t timeout_ms);

esp_err_t hal_i2c_master_device_read(hal_i2c_device_t *dev,
                                     uint8_t *data,
                                     size_t len,
                                     uint32_t timeout_ms);

esp_err_t hal_i2c_master_device_write_read(hal_i2c_device_t *dev,
                                           const uint8_t *wdata,
                                           size_t wlen,
                                           uint8_t *rdata,
                                           size_t rlen,
                                           uint32_t timeout_ms);

esp_err_t hal_i2c_master_probe(hal_i2c_bus_t *bus,
                               uint8_t addr,
                               uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
