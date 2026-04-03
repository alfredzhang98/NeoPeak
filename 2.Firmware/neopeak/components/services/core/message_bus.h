#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MSG_TOPIC_IMU_RAW = 1,
    MSG_TOPIC_IMU_QUAT = 2,
    MSG_TOPIC_ENCODER = 3,
} message_topic_t;

typedef struct {
    message_topic_t topic;
    const void *data;
    size_t size;
    uint32_t timestamp_ms;
} message_t;

typedef void (*message_callback_t)(const message_t *msg, void *user_ctx);

/**
 * @brief 初始化消息总线
 * @return 初始化结果
 */
esp_err_t message_bus_init(void);

/**
 * @brief 订阅消息主题
 * @param topic 主题
 * @param cb 回调函数
 * @param user_ctx 用户上下文
 * @return 订阅结果
 */
esp_err_t message_bus_subscribe(message_topic_t topic, message_callback_t cb, void *user_ctx);

/**
 * @brief 取消订阅
 * @param topic 主题
 * @param cb 回调函数
 * @param user_ctx 用户上下文
 */
void message_bus_unsubscribe(message_topic_t topic, message_callback_t cb, void *user_ctx);

/**
 * @brief 发布消息（同步分发，回调中如需持久化数据请自行拷贝）
 * @param topic 主题
 * @param data 数据指针
 * @param size 数据长度
 */
void message_bus_publish(message_topic_t topic, const void *data, size_t size);

#ifdef __cplusplus
}
#endif
