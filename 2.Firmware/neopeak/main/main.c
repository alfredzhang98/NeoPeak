/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_init.h"
#include "display_service.h"
#include "imu_service.h"
#include "sdkconfig.h"

static const char *TAG = "main";

void app_main(void)
{
    board_init();

    esp_err_t err = display_service_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "display service start failed: %s", esp_err_to_name(err));
    }

    const imu_service_config_t imu_cfg = {
        .sda_pin = CONFIG_MCU_SDA_PIN,
        .scl_pin = CONFIG_MCU_SCL_PIN,
        .i2c_addr = CONFIG_IMU_I2C_ADDR,
        .i2c_clk_hz = CONFIG_IMU_I2C_HZ,
        .sample_period_ms = CONFIG_IMU_SAMPLE_PERIOD_MS,
    };
    err = imu_service_start(&imu_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "imu service start failed: %s", esp_err_to_name(err));
    }

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
