#include "imu_i2c_tool.h"

#include <stdint.h>

#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "hal_i2c.h"

namespace {
constexpr const char *kTag = "imu_i2c_tool";
constexpr uint8_t kWhoAmIReg = 0x75;

void log_probe(hal_i2c_bus_t *bus, uint8_t addr)
{
    esp_err_t err = hal_i2c_master_probe(bus, addr, 50);
    if (err == ESP_OK) {
        ESP_LOGI(kTag, "i2c probe 0x%02X: ACK", addr);
    } else {
        ESP_LOGI(kTag, "i2c probe 0x%02X: %s", addr, esp_err_to_name(err));
    }
}

void read_whoami(hal_i2c_bus_t *bus, uint8_t addr, uint32_t i2c_hz)
{
    hal_i2c_device_t dev = {};
    esp_err_t err = hal_i2c_master_device_add(bus, addr, i2c_hz, &dev);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "add device 0x%02X failed: %s", addr, esp_err_to_name(err));
        return;
    }

    uint8_t whoami = 0;
    err = hal_i2c_master_device_write_read(&dev, &kWhoAmIReg, 1, &whoami, 1, 50);
    if (err == ESP_OK) {
        ESP_LOGI(kTag, "whoami @0x%02X = 0x%02X", addr, whoami);
    } else {
        ESP_LOGE(kTag, "whoami read @0x%02X failed: %s", addr, esp_err_to_name(err));
    }

    hal_i2c_master_device_remove(&dev);
}
} // namespace

extern "C" void imu_i2c_tool_run(void)
{
    ESP_LOGI(kTag, "imu i2c tool start");

    hal_i2c_bus_t bus = {};
    esp_err_t err = hal_i2c_master_bus_init(&bus,
                                            I2C_NUM_0,
                                            (gpio_num_t)CONFIG_MCU_SDA_PIN,
                                            (gpio_num_t)CONFIG_MCU_SCL_PIN,
                                            (uint32_t)CONFIG_IMU_I2C_TOOL_HZ);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "i2c bus init failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(kTag, "i2c tool hz=%u", (unsigned)CONFIG_IMU_I2C_TOOL_HZ);

    log_probe(&bus, 0x68);
    log_probe(&bus, 0x69);

    read_whoami(&bus, (uint8_t)CONFIG_IMU_I2C_ADDR, (uint32_t)CONFIG_IMU_I2C_TOOL_HZ);

    hal_i2c_master_bus_deinit(&bus);
    ESP_LOGI(kTag, "imu i2c tool done");
}
