/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_init.h"
#include "imu_i2c_tool.h"
#include "sdkconfig.h"

void app_main(void)
{
#if CONFIG_IMU_I2C_TOOL
    imu_i2c_tool_run();

#if CONFIG_IMU_I2C_TOOL_ONLY
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif
#endif

    board_init();

    // app_main 不能返回，否则会触发 WDT 复位
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
