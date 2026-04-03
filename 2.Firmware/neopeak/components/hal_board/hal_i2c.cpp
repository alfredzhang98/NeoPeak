#include "hal_i2c.h"

// Board-specific implementation: replace I2C bus setup here when porting to a new board.

esp_err_t hal_i2c_master_bus_init(hal_i2c_bus_t *bus,
                                  i2c_port_t port,
                                  gpio_num_t sda_gpio,
                                  gpio_num_t scl_gpio,
                                  uint32_t clk_hz)
{
    if (bus == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // I2C 主机总线配置
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = port,
        .sda_io_num = sda_gpio,
        .scl_io_num = scl_gpio,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 4,
        .flags = {
            .enable_internal_pullup = 1,
            .allow_pd = 0,
        },
    };

    return i2c_new_master_bus(&bus_cfg, &bus->handle);
}

void hal_i2c_master_bus_deinit(hal_i2c_bus_t *bus)
{
    if (bus == NULL || bus->handle == NULL) {
        return;
    }

    i2c_del_master_bus(bus->handle);
    bus->handle = NULL;
}

esp_err_t hal_i2c_master_device_add(hal_i2c_bus_t *bus,
                                    uint8_t addr,
                                    uint32_t clk_hz,
                                    hal_i2c_device_t *dev)
{
    if (bus == NULL || bus->handle == NULL || dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // I2C 设备配置
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = clk_hz,
        .scl_wait_us = 0,
        .flags = {
            .disable_ack_check = 0,
        },
    };

    return i2c_master_bus_add_device(bus->handle, &dev_cfg, &dev->handle);
}

void hal_i2c_master_device_remove(hal_i2c_device_t *dev)
{
    if (dev == NULL || dev->handle == NULL) {
        return;
    }

    esp_err_t err = i2c_master_bus_rm_device(dev->handle);
    if (err == ESP_OK) {
        dev->handle = NULL;
    }
}

esp_err_t hal_i2c_master_device_write(hal_i2c_device_t *dev,
                                      const uint8_t *data,
                                      size_t len,
                                      uint32_t timeout_ms)
{
    if (dev == NULL || dev->handle == NULL || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_transmit(dev->handle, data, len, timeout_ms);
}

esp_err_t hal_i2c_master_device_read(hal_i2c_device_t *dev,
                                     uint8_t *data,
                                     size_t len,
                                     uint32_t timeout_ms)
{
    if (dev == NULL || dev->handle == NULL || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_receive(dev->handle, data, len, timeout_ms);
}

esp_err_t hal_i2c_master_device_write_read(hal_i2c_device_t *dev,
                                           const uint8_t *wdata,
                                           size_t wlen,
                                           uint8_t *rdata,
                                           size_t rlen,
                                           uint32_t timeout_ms)
{
    if (dev == NULL || dev->handle == NULL || wdata == NULL || wlen == 0 || rdata == NULL || rlen == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_transmit_receive(dev->handle, wdata, wlen, rdata, rlen, timeout_ms);
}

esp_err_t hal_i2c_master_probe(hal_i2c_bus_t *bus, uint8_t addr, uint32_t timeout_ms)
{
    if (bus == NULL || bus->handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_probe(bus->handle, addr, (int)timeout_ms);
}
