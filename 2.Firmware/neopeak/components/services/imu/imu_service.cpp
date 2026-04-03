#include "imu_service.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "imu42688p.hpp"
#include "hal_i2c.h"
#include "message_bus.h"

namespace {
constexpr uint32_t kSamplePeriodMs = 20;

Imu42688p s_imu;
hal_i2c_bus_t s_bus = {};
TaskHandle_t s_task = nullptr;
bool s_started = false;

void imu_task(void *arg)
{
    (void)arg;
    imu_raw_sample_t sample = {};
    Imu42688p::Vec3f accel = {};
    Imu42688p::Vec3f gyro = {};
    float temp_c = 0.0f;

    while (true) {
        if (s_imu.read_all(&accel, &gyro, &temp_c) == ESP_OK) {
            sample.timestamp_ms = (uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS;
            sample.accel_mg[0] = accel.x;
            sample.accel_mg[1] = accel.y;
            sample.accel_mg[2] = accel.z;
            sample.gyro_dps[0] = gyro.x;
            sample.gyro_dps[1] = gyro.y;
            sample.gyro_dps[2] = gyro.z;
            sample.temp_c = temp_c;

            message_bus_publish(MSG_TOPIC_IMU_RAW, &sample, sizeof(sample));
        }

        vTaskDelay(pdMS_TO_TICKS(kSamplePeriodMs));
    }
}
} // namespace

esp_err_t imu_service_start(const imu_service_config_t *cfg)
{
    if (s_started) {
        return ESP_OK;
    }

    if (cfg == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = hal_i2c_master_bus_init(&s_bus,
                                            cfg->port,
                                            cfg->sda_gpio,
                                            cfg->scl_gpio,
                                            cfg->i2c_hz);
    if (err != ESP_OK) {
        return err;
    }

    err = s_imu.begin(&s_bus, cfg->addr, cfg->i2c_hz);
    if (err != ESP_OK) {
        hal_i2c_master_bus_deinit(&s_bus);
        return err;
    }

    s_started = true;
    xTaskCreate(imu_task, "imu_service", 4096, nullptr, 5, &s_task);
    return ESP_OK;
}
