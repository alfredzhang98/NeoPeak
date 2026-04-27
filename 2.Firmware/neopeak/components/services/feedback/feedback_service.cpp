#include "feedback_service.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "buzzer.hpp"
#include "motor.h"
#include "tone_music.h"
#include "tone_player.h"
#include "tone_utils.h"
#include "message_bus.h"

namespace {
constexpr const char *kTag    = "feedback";
constexpr size_t      kQueueDepth = 8;

struct FeedbackItem {
    feedback_request_t req;
    SemaphoreHandle_t  done_sem; // nullptr = fire-and-forget
};

QueueHandle_t     s_queue   = nullptr;
TaskHandle_t      s_task    = nullptr;
bool              s_started = false;
uint8_t           s_haptic_percent = 60;

motor_intensity_t scale_haptic_intensity(int base)
{
    if (s_haptic_percent == 0) {
        return MOTOR_INTENSITY_1;
    }

    int scaled = (base * (int)s_haptic_percent + 99) / 100;
    if (scaled < 1) {
        scaled = 1;
    }
    if (scaled > 10) {
        scaled = 10;
    }
    return (motor_intensity_t)scaled;
}

void tone_callback(uint32_t freq_hz, uint16_t volume)
{
    (void)volume;
    buzzer_tone(freq_hz, 0);
}

void do_haptic(feedback_haptic_t haptic)
{
    if (s_haptic_percent == 0) {
        return;
    }

    switch (haptic) {
    case FEEDBACK_HAPTIC_LIGHT:  pwm_motor_shake(300, scale_haptic_intensity(3)); break;
    case FEEDBACK_HAPTIC_MEDIUM: pwm_motor_shake(400, scale_haptic_intensity(5)); break;
    case FEEDBACK_HAPTIC_STRONG: pwm_motor_shake(500, scale_haptic_intensity(8)); break;
    default: break;
    }
}

void feedback_task(void *arg)
{
    (void)arg;

    const TickType_t scan_delay = pdMS_TO_TICKS(10);
    TonePlayer player;
    player.SetCallback(tone_callback);

    FeedbackItem item = {};

    while (true) {
        if (xQueueReceive(s_queue, &item, pdMS_TO_TICKS(10)) == pdTRUE) {
            const feedback_request_t &req = item.req;

            do_haptic(req.haptic);

            if (req.music != FEEDBACK_MUSIC_NONE) {
                tone_play_and_wait(player, static_cast<MusicId>(req.music), scan_delay);
            }

            if (item.done_sem != nullptr) {
                xSemaphoreGive(item.done_sem);
            }
        }

        player.Update((uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS);
    }
}

void on_feedback_msg(const message_t *msg, void *user_ctx)
{
    (void)user_ctx;

    if (msg == nullptr || msg->data == nullptr || msg->size < sizeof(feedback_request_t)) {
        return;
    }

    FeedbackItem item = {};
    item.req      = *(const feedback_request_t *)msg->data;
    item.done_sem = nullptr;
    xQueueSend(s_queue, &item, 0);
}
} // namespace

esp_err_t feedback_service_start(void)
{
    if (s_started) {
        return ESP_OK;
    }

    s_queue = xQueueCreate(kQueueDepth, sizeof(FeedbackItem));
    if (s_queue == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    message_bus_subscribe(MSG_TOPIC_FEEDBACK, on_feedback_msg, nullptr);

    s_started = true;
    xTaskCreate(feedback_task, "feedback", 4096, nullptr, 5, &s_task);
    ESP_LOGI(kTag, "started");
    return ESP_OK;
}

void feedback_post(feedback_music_t music, feedback_haptic_t haptic)
{
    if (s_queue == nullptr) {
        return;
    }

    FeedbackItem item = {};
    item.req.music  = music;
    item.req.haptic = haptic;
    item.done_sem   = nullptr;
    xQueueSend(s_queue, &item, 0);
}

void feedback_post_blocking(feedback_music_t music, feedback_haptic_t haptic)
{
    if (s_queue == nullptr) {
        return;
    }

    SemaphoreHandle_t sem = xSemaphoreCreateBinary();
    if (sem == nullptr) {
        feedback_post(music, haptic);
        return;
    }

    FeedbackItem item = {};
    item.req.music  = music;
    item.req.haptic = haptic;
    item.done_sem   = sem;

    if (xQueueSend(s_queue, &item, pdMS_TO_TICKS(100)) == pdTRUE) {
        xSemaphoreTake(sem, portMAX_DELAY);
    }

    vSemaphoreDelete(sem);
}

void feedback_service_set_volume_percent(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    buzzer_set_volume_percent(percent);
}

void feedback_service_set_haptic_percent(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    s_haptic_percent = percent;
}
