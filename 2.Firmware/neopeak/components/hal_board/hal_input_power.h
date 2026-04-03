#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int power_en_pin;
    int charge_det_pin;
} hal_input_power_config_t;

/**
 * @brief 初始化电源相关 GPIO
 * @param cfg GPIO 配置
 * @return 初始化结果
 */
esp_err_t hal_input_power_init(const hal_input_power_config_t *cfg);

/**
 * @brief 设置电源使能脚电平
 * @param enable true 拉高，false 拉低
 */
void hal_input_power_set_enable(bool enable);

/**
 * @brief 读取充电检测电平
 * @return 充电检测电平，失败返回 -1
 */
int hal_input_power_get_charge_level(void);

#ifdef __cplusplus
}
#endif
