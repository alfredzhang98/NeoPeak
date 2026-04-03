#include "message_bus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace {
constexpr size_t kMaxSubscribers = 16;

struct Subscriber {
    message_topic_t topic;
    message_callback_t cb;
    void *user_ctx;
};

SemaphoreHandle_t s_lock = nullptr;
Subscriber s_subscribers[kMaxSubscribers] = {};
size_t s_sub_count = 0;

uint32_t get_timestamp_ms()
{
    return (uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS;
}
} // namespace

esp_err_t message_bus_init(void)
{
    if (s_lock != nullptr) {
        return ESP_OK;
    }

    s_lock = xSemaphoreCreateMutex();
    return (s_lock != nullptr) ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t message_bus_subscribe(message_topic_t topic, message_callback_t cb, void *user_ctx)
{
    if (cb == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = message_bus_init();
    if (err != ESP_OK) {
        return err;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (size_t i = 0; i < s_sub_count; ++i) {
        if (s_subscribers[i].topic == topic && s_subscribers[i].cb == cb && s_subscribers[i].user_ctx == user_ctx) {
            xSemaphoreGive(s_lock);
            return ESP_ERR_INVALID_STATE;
        }
    }

    if (s_sub_count >= kMaxSubscribers) {
        xSemaphoreGive(s_lock);
        return ESP_ERR_NO_MEM;
    }

    s_subscribers[s_sub_count++] = {topic, cb, user_ctx};
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

void message_bus_unsubscribe(message_topic_t topic, message_callback_t cb, void *user_ctx)
{
    if (s_lock == nullptr || cb == nullptr) {
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (size_t i = 0; i < s_sub_count; ++i) {
        if (s_subscribers[i].topic == topic && s_subscribers[i].cb == cb && s_subscribers[i].user_ctx == user_ctx) {
            for (size_t j = i + 1; j < s_sub_count; ++j) {
                s_subscribers[j - 1] = s_subscribers[j];
            }
            --s_sub_count;
            break;
        }
    }
    xSemaphoreGive(s_lock);
}

void message_bus_publish(message_topic_t topic, const void *data, size_t size)
{
    if (s_lock == nullptr) {
        if (message_bus_init() != ESP_OK) {
            return;
        }
    }

    Subscriber local_list[kMaxSubscribers];
    size_t local_count = 0;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    local_count = s_sub_count;
    for (size_t i = 0; i < local_count; ++i) {
        local_list[i] = s_subscribers[i];
    }
    xSemaphoreGive(s_lock);

    const message_t msg = {
        .topic = topic,
        .data = data,
        .size = size,
        .timestamp_ms = get_timestamp_ms(),
    };

    for (size_t i = 0; i < local_count; ++i) {
        if (local_list[i].topic == topic && local_list[i].cb != nullptr) {
            local_list[i].cb(&msg, local_list[i].user_ctx);
        }
    }
}
