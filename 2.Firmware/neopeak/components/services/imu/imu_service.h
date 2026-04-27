#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "icm42688p.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int sda_pin;
    int scl_pin;
    uint8_t i2c_addr;
    uint32_t i2c_clk_hz;
    uint32_t sample_period_ms;
} imu_service_config_t;

typedef struct {
    float q[4]; // w, x, y, z
    float roll_rad;
    float pitch_rad;
    float yaw_rad; // Relative yaw from gyro integration; it will drift without an external heading reference.
    uint8_t calib_percent;     // 0..100 — gyro-bias calibration progress; 100 = ready
    float gyro_bias_rad_s[3];  // estimated per-axis bias used for compensation (valid once calib_percent == 100)
} imu_attitude_t;

typedef struct {
    uint32_t timestamp_ms;
    icm42688p_raw_data_t raw;
    icm42688p_data_t data;
    imu_attitude_t attitude;
} imu_sample_t;

esp_err_t imu_service_start(const imu_service_config_t *cfg);
void imu_service_set_enabled(bool enabled);

#ifdef __cplusplus
}
#endif
