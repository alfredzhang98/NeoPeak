#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ENCODER_EVENT_ROTATE = 0,
    ENCODER_EVENT_BUTTON = 1,
} encoder_event_type_t;

typedef struct {
    encoder_event_type_t type;
    int8_t delta;
    bool pressed;
} encoder_event_t;

typedef struct {
    int pin_a;
    int pin_b;
    int pin_button;
    bool button_active_low;
    uint32_t poll_ms;
    uint32_t debounce_ms;
} encoder_service_config_t;

/**
 * @brief 启动编码器服务（轮询 A/B + 按键并发布 MSG_TOPIC_ENCODER）
 * @param cfg 服务配置
 * @return 启动结果
 */
esp_err_t encoder_service_start(const encoder_service_config_t *cfg);
bool encoder_service_is_button_pressed(void);

#ifdef __cplusplus
}
#endif
