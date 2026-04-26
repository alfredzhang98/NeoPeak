#pragma once

#include "esp_err.h"

#include "hal_input_power.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    hal_input_power_config_t io;
    uint32_t power_off_long_press_ms;
} power_service_config_t;

/**
 * @brief 启动电源管理服务（按键长按、开关机、提示音/震动）
 * @param cfg 服务配置
 * @return 启动结果
 */
esp_err_t power_service_start(const power_service_config_t *cfg);

#ifdef __cplusplus
}
#endif
