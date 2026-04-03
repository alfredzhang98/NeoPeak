#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int pin_a;
    int pin_b;
    int pin_button;
    bool button_active_low;
} hal_encoder_config_t;

/**
 * @brief 初始化编码器 GPIO（A/B + 按键）
 * @param cfg GPIO 配置
 * @return 初始化结果
 */
esp_err_t hal_encoder_init(const hal_encoder_config_t *cfg);

/**
 * @brief 读取 A 相电平
 * @return 电平，失败返回 -1
 */
int hal_encoder_get_a_level(void);

/**
 * @brief 读取 B 相电平
 * @return 电平，失败返回 -1
 */
int hal_encoder_get_b_level(void);

/**
 * @brief 读取按键电平
 * @return 电平，失败返回 -1
 */
int hal_encoder_get_button_level(void);

/**
 * @brief 按键是否按下（按 active_low 判断）
 * @return 按下返回 true
 */
bool hal_encoder_is_button_pressed(void);

#ifdef __cplusplus
}
#endif
