#include "power_service.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "hal_input_power.h"
#include "encoder_service.h"
#include "feedback_service.h"
#include "message_bus.h"

namespace {
constexpr const char *kTag = "power_service";

TaskHandle_t  s_task            = nullptr;
bool          s_started         = false;
TickType_t    s_off_press_ticks = 0;
QueueHandle_t s_evt_queue       = nullptr;
bool          s_button_pressed  = false;
TickType_t    s_press_start     = 0;

void on_encoder_event(const message_t *msg, void *user_ctx)
{
    (void)user_ctx;

    if (msg == nullptr || msg->data == nullptr || msg->size < sizeof(encoder_event_t)) {
        return;
    }

    const encoder_event_t *evt = (const encoder_event_t *)msg->data;
    if (evt->type != ENCODER_EVENT_BUTTON) {
        return;
    }

    if (s_evt_queue != nullptr) {
        xQueueSend(s_evt_queue, evt, 0);
    }
}

void power_task(void *arg)
{
    (void)arg;

    feedback_post_blocking(FEEDBACK_MUSIC_STARTUP, FEEDBACK_HAPTIC_STRONG);
    ESP_LOGI(kTag, "startup complete");

    constexpr TickType_t kChargeDebounceMs = 500;

    int last_charge_level    = hal_input_power_get_charge_level();
    int pending_charge_level = last_charge_level;
    TickType_t charge_change_at = 0;

    while (true) {
        encoder_event_t evt = {};
        while (s_evt_queue != nullptr && xQueueReceive(s_evt_queue, &evt, 0) == pdTRUE) {
            if (evt.type == ENCODER_EVENT_BUTTON) {
                s_button_pressed = evt.pressed;
                s_press_start    = s_button_pressed ? xTaskGetTickCount() : 0;
            }
        }

        int chg_level = hal_input_power_get_charge_level();
        if (chg_level >= 0) {
            if (chg_level != pending_charge_level) {
                // Level changed — restart debounce timer
                pending_charge_level = chg_level;
                charge_change_at     = xTaskGetTickCount();
            } else if (pending_charge_level != last_charge_level) {
                // Level stable — fire only after debounce window elapses
                if ((xTaskGetTickCount() - charge_change_at) >= pdMS_TO_TICKS(kChargeDebounceMs)) {
                    last_charge_level = pending_charge_level;
                    if (last_charge_level == 0) {
                        feedback_post(FEEDBACK_MUSIC_CHARGE_START, FEEDBACK_HAPTIC_MEDIUM);
                    } else {
                        feedback_post(FEEDBACK_MUSIC_CHARGE_END, FEEDBACK_HAPTIC_NONE);
                    }
                }
            }
        }

        if (s_button_pressed && s_press_start != 0) {
            TickType_t elapsed = xTaskGetTickCount() - s_press_start;
            if (elapsed >= s_off_press_ticks) {
                ESP_LOGI(kTag, "shutdown");
                feedback_post_blocking(FEEDBACK_MUSIC_SHUTDOWN, FEEDBACK_HAPTIC_STRONG);
                hal_input_power_set_enable(false);
                s_press_start = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
} // namespace

esp_err_t power_service_start(const power_service_config_t *cfg)
{
    if (s_started) {
        return ESP_OK;
    }

    if (cfg == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = hal_input_power_init(&cfg->io);
    if (err != ESP_OK) {
        return err;
    }

    s_off_press_ticks = pdMS_TO_TICKS(cfg->power_off_long_press_ms);

    s_evt_queue = xQueueCreate(8, sizeof(encoder_event_t));
    if (s_evt_queue == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    message_bus_subscribe(MSG_TOPIC_ENCODER, on_encoder_event, nullptr);

    s_started = true;
    xTaskCreate(power_task, "power_service", 3072, nullptr, 5, &s_task);
    return ESP_OK;
}
