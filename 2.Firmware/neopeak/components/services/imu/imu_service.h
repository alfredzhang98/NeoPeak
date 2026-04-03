#pragma once

#include <stdint.h>

#include "esp_err.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t timestamp_ms;
    float accel_mg[3];
    float gyro_dps[3];
    float temp_c;
} imu_raw_sample_t;

typedef struct {
    i2c_port_t port;
    gpio_num_t sda_gpio;
    gpio_num_t scl_gpio;
    uint32_t i2c_hz;
    uint8_t addr;
} imu_service_config_t;

/**
 * @brief 启动 IMU 采集服务（定时采样并发布 MSG_TOPIC_IMU_RAW）
 * @param cfg 服务配置
 * @return 启动结果
 */
esp_err_t imu_service_start(const imu_service_config_t *cfg);

#ifdef __cplusplus
}
#endif
