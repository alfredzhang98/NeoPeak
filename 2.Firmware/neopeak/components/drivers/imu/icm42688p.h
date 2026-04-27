#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "hal_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ICM42688P_I2C_ADDR_AD0_LOW  0x68
#define ICM42688P_I2C_ADDR_AD0_HIGH 0x69
#define ICM42688P_WHO_AM_I_VALUE    0x47

typedef enum {
    ICM42688P_GYRO_FS_2000DPS = 0,
    ICM42688P_GYRO_FS_1000DPS = 1,
    ICM42688P_GYRO_FS_500DPS = 2,
    ICM42688P_GYRO_FS_250DPS = 3,
    ICM42688P_GYRO_FS_125DPS = 4,
    ICM42688P_GYRO_FS_62_5DPS = 5,
    ICM42688P_GYRO_FS_31_25DPS = 6,
    ICM42688P_GYRO_FS_15_625DPS = 7,
} icm42688p_gyro_fs_t;

typedef enum {
    ICM42688P_ACCEL_FS_16G = 0,
    ICM42688P_ACCEL_FS_8G = 1,
    ICM42688P_ACCEL_FS_4G = 2,
    ICM42688P_ACCEL_FS_2G = 3,
} icm42688p_accel_fs_t;

typedef enum {
    ICM42688P_ODR_32KHZ = 1,
    ICM42688P_ODR_16KHZ = 2,
    ICM42688P_ODR_8KHZ = 3,
    ICM42688P_ODR_4KHZ = 4,
    ICM42688P_ODR_2KHZ = 5,
    ICM42688P_ODR_1KHZ = 6,
    ICM42688P_ODR_200HZ = 7,
    ICM42688P_ODR_100HZ = 8,
    ICM42688P_ODR_50HZ = 9,
    ICM42688P_ODR_25HZ = 10,
    ICM42688P_ODR_12_5HZ = 11,
    ICM42688P_ODR_6_25HZ = 12,
    ICM42688P_ODR_3_125HZ = 13,
    ICM42688P_ODR_1_5625HZ = 14,
    ICM42688P_ODR_500HZ = 15,
} icm42688p_odr_t;

typedef enum {
    ICM42688P_GYRO_MODE_OFF = 0,
    ICM42688P_GYRO_MODE_STANDBY = 1,
    ICM42688P_GYRO_MODE_LOW_NOISE = 3,
} icm42688p_gyro_mode_t;

typedef enum {
    ICM42688P_ACCEL_MODE_OFF = 0,
    ICM42688P_ACCEL_MODE_LOW_POWER = 2,
    ICM42688P_ACCEL_MODE_LOW_NOISE = 3,
} icm42688p_accel_mode_t;

typedef enum {
    ICM42688P_UI_FILTER_ORDER_1 = 0,
    ICM42688P_UI_FILTER_ORDER_2 = 1,
    ICM42688P_UI_FILTER_ORDER_3 = 2,
} icm42688p_ui_filter_order_t;

typedef enum {
    ICM42688P_UI_FILTER_BW_ODR_DIV_2 = 0,
    ICM42688P_UI_FILTER_BW_MAX_400_ODR_DIV_4 = 1,
    ICM42688P_UI_FILTER_BW_MAX_400_ODR_DIV_5 = 2,
    ICM42688P_UI_FILTER_BW_MAX_400_ODR_DIV_8 = 3,
    ICM42688P_UI_FILTER_BW_MAX_400_ODR_DIV_10 = 4,
    ICM42688P_UI_FILTER_BW_MAX_400_ODR_DIV_16 = 5,
    ICM42688P_UI_FILTER_BW_MAX_400_ODR_DIV_20 = 6,
    ICM42688P_UI_FILTER_BW_MAX_400_ODR_DIV_40 = 7,
    ICM42688P_UI_FILTER_BW_LOW_LATENCY_DEC2 = 14,
    ICM42688P_UI_FILTER_BW_LOW_LATENCY_8X = 15,
} icm42688p_ui_filter_bw_t;

typedef enum {
    ICM42688P_INT_MODE_PULSED = 0,
    ICM42688P_INT_MODE_LATCHED = 1,
} icm42688p_int_mode_t;

typedef enum {
    ICM42688P_INT_DRIVE_OPEN_DRAIN = 0,
    ICM42688P_INT_DRIVE_PUSH_PULL = 1,
} icm42688p_int_drive_t;

typedef enum {
    ICM42688P_INT_POLARITY_ACTIVE_LOW = 0,
    ICM42688P_INT_POLARITY_ACTIVE_HIGH = 1,
} icm42688p_int_polarity_t;

typedef enum {
    ICM42688P_INT_PIN_1 = 1,
    ICM42688P_INT_PIN_2 = 2,
} icm42688p_int_pin_t;

typedef enum {
    ICM42688P_INT_SOURCE_UI_AGC_RDY = 0,
    ICM42688P_INT_SOURCE_FIFO_FULL,
    ICM42688P_INT_SOURCE_FIFO_THS,
    ICM42688P_INT_SOURCE_UI_DRDY,
    ICM42688P_INT_SOURCE_RESET_DONE,
    ICM42688P_INT_SOURCE_PLL_RDY,
    ICM42688P_INT_SOURCE_UI_FSYNC,
    ICM42688P_INT_SOURCE_TAP,
    ICM42688P_INT_SOURCE_SLEEP,
    ICM42688P_INT_SOURCE_WAKE,
    ICM42688P_INT_SOURCE_TILT,
    ICM42688P_INT_SOURCE_STEP_COUNT_OVERFLOW,
    ICM42688P_INT_SOURCE_STEP_DETECT,
} icm42688p_int_source_t;

typedef enum {
    ICM42688P_TAP_NONE = 0,
    ICM42688P_TAP_SINGLE = 1,
    ICM42688P_TAP_DOUBLE = 2,
} icm42688p_tap_num_t;

typedef enum {
    ICM42688P_TAP_AXIS_X = 0,
    ICM42688P_TAP_AXIS_Y = 1,
    ICM42688P_TAP_AXIS_Z = 2,
} icm42688p_tap_axis_t;

typedef struct {
    uint8_t min_jerk_thr;
    uint8_t max_peak_tol;
    uint8_t tmax;
    uint8_t tavg;
    uint8_t tmin;
} icm42688p_tap_config_t;

typedef struct {
    hal_i2c_device_t *dev;
    uint32_t timeout_ms;
    bool reset_on_begin;
    bool configure_i2c_interface;
    bool enable_temp;
    icm42688p_gyro_fs_t gyro_fs;
    icm42688p_accel_fs_t accel_fs;
    icm42688p_odr_t gyro_odr;
    icm42688p_odr_t accel_odr;
    icm42688p_gyro_mode_t gyro_mode;
    icm42688p_accel_mode_t accel_mode;
} icm42688p_config_t;

typedef struct {
    hal_i2c_device_t *dev;
    uint32_t timeout_ms;
    uint8_t current_bank;
    icm42688p_gyro_fs_t gyro_fs;
    icm42688p_accel_fs_t accel_fs;
    icm42688p_gyro_mode_t gyro_mode;
    icm42688p_accel_mode_t accel_mode;
    bool temp_enabled;
    bool initialized;
} icm42688p_t;

typedef struct {
    int16_t temp;
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} icm42688p_raw_data_t;

typedef struct {
    float temp_c;
    float accel_g[3];
    float gyro_dps[3];
} icm42688p_data_t;

typedef struct {
    icm42688p_tap_num_t num;
    icm42688p_tap_axis_t axis;
    bool negative;
    uint8_t raw;
} icm42688p_tap_event_t;

void icm42688p_config_default(icm42688p_config_t *cfg, hal_i2c_device_t *dev);

esp_err_t icm42688p_begin(icm42688p_t *imu, const icm42688p_config_t *cfg);
void icm42688p_deinit(icm42688p_t *imu);

esp_err_t icm42688p_read_reg(icm42688p_t *imu, uint8_t bank, uint8_t reg, uint8_t *value);
esp_err_t icm42688p_write_reg(icm42688p_t *imu, uint8_t bank, uint8_t reg, uint8_t value);
esp_err_t icm42688p_read_regs(icm42688p_t *imu, uint8_t bank, uint8_t reg, uint8_t *data, uint16_t len);

esp_err_t icm42688p_soft_reset(icm42688p_t *imu);
esp_err_t icm42688p_read_who_am_i(icm42688p_t *imu, uint8_t *who_am_i);
esp_err_t icm42688p_check_who_am_i(icm42688p_t *imu);

esp_err_t icm42688p_set_sensor_config(icm42688p_t *imu,
                                      icm42688p_gyro_fs_t gyro_fs,
                                      icm42688p_accel_fs_t accel_fs,
                                      icm42688p_odr_t gyro_odr,
                                      icm42688p_odr_t accel_odr);
esp_err_t icm42688p_set_power(icm42688p_t *imu,
                              icm42688p_gyro_mode_t gyro_mode,
                              icm42688p_accel_mode_t accel_mode,
                              bool temp_enable);
esp_err_t icm42688p_set_ui_filter(icm42688p_t *imu,
                                  icm42688p_ui_filter_bw_t gyro_bw,
                                  icm42688p_ui_filter_bw_t accel_bw,
                                  icm42688p_ui_filter_order_t gyro_order,
                                  icm42688p_ui_filter_order_t accel_order);
esp_err_t icm42688p_config_int(icm42688p_t *imu,
                               icm42688p_int_pin_t pin,
                               icm42688p_int_mode_t mode,
                               icm42688p_int_drive_t drive,
                               icm42688p_int_polarity_t polarity);
esp_err_t icm42688p_config_int1(icm42688p_t *imu,
                                icm42688p_int_mode_t mode,
                                icm42688p_int_drive_t drive,
                                icm42688p_int_polarity_t polarity);
esp_err_t icm42688p_route_interrupt(icm42688p_t *imu,
                                    icm42688p_int_pin_t pin,
                                    icm42688p_int_source_t source,
                                    bool enable);
esp_err_t icm42688p_apex_init(icm42688p_t *imu);
esp_err_t icm42688p_tap_config_default(icm42688p_tap_config_t *cfg);
esp_err_t icm42688p_set_tap_config(icm42688p_t *imu, const icm42688p_tap_config_t *cfg);
esp_err_t icm42688p_set_tap_enable(icm42688p_t *imu, bool enable);
esp_err_t icm42688p_enable_tap_int1(icm42688p_t *imu, const icm42688p_tap_config_t *cfg);
esp_err_t icm42688p_disable_tap(icm42688p_t *imu);
esp_err_t icm42688p_read_tap_event(icm42688p_t *imu, icm42688p_tap_event_t *event);
esp_err_t icm42688p_read_int_status(icm42688p_t *imu, uint8_t *status);
esp_err_t icm42688p_read_int_status3(icm42688p_t *imu, uint8_t *status3);
esp_err_t icm42688p_data_ready(icm42688p_t *imu, bool *ready);
esp_err_t icm42688p_flush_fifo(icm42688p_t *imu);

esp_err_t icm42688p_read_raw(icm42688p_t *imu, icm42688p_raw_data_t *raw);
esp_err_t icm42688p_read(icm42688p_t *imu, icm42688p_data_t *data);

float icm42688p_raw_temp_to_c(int16_t raw_temp);
float icm42688p_accel_lsb_per_g(icm42688p_accel_fs_t fs);
float icm42688p_gyro_lsb_per_dps(icm42688p_gyro_fs_t fs);

#ifdef __cplusplus
}
#endif
