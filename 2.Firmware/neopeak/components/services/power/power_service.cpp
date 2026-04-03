#include "power_service.h"

#include "esp_log.h"
#include "esp_psram.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "buzzer.hpp"
#include "tone_player.h"
#include "tone_utils.h"

#include "motor.h"

#include "hal_input_power.h"
#include "encoder_service.h"
#include "message_bus.h"

namespace {
TaskHandle_t s_task = nullptr;
bool s_started = false;
TickType_t s_on_press_ticks = 0;
TickType_t s_off_press_ticks = 0;
QueueHandle_t s_evt_queue = nullptr;
bool s_button_pressed = false;
TickType_t s_press_start = 0;
constexpr const char *kPsramTag = "psram";

void log_psram_size(const char *phase)
{
#if defined(CONFIG_SPIRAM) && CONFIG_SPIRAM
    size_t size = esp_psram_get_size();
    ESP_LOGI(kPsramTag, "%s psram size: %u bytes", phase, (unsigned)size);
#else
    ESP_LOGI(kPsramTag, "%s psram disabled", phase);
#endif
}

void tone_callback(uint32_t freq_hz, uint16_t volume)
{
    (void)volume;
    buzzer_tone(freq_hz, 0);
}


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

    const TickType_t scan_delay = pdMS_TO_TICKS(10);

    TonePlayer player;
    player.SetCallback(tone_callback);

    bool power_on = false;
    int last_charge_level = hal_input_power_get_charge_level();

    while (true) {
        encoder_event_t evt = {};
        while (s_evt_queue != nullptr && xQueueReceive(s_evt_queue, &evt, 0) == pdTRUE) {
            if (evt.type == ENCODER_EVENT_BUTTON) {
                s_button_pressed = evt.pressed;
                if (s_button_pressed) {
                    s_press_start = xTaskGetTickCount();
                } else {
                    s_press_start = 0;
                }
            }
        }

        int chg_level = hal_input_power_get_charge_level();
        if (chg_level != last_charge_level && chg_level >= 0) {
            last_charge_level = chg_level;
            if (chg_level == 0) {
                pwm_motor_shake(500, MOTOR_INTENSITY_3);
                tone_play_and_wait(player, MusicId::BattChargeStart, scan_delay);
            } else {
                tone_play_and_wait(player, MusicId::BattChargeEnd, scan_delay);
            }
        }

        if (s_button_pressed && s_press_start != 0) {
            TickType_t elapsed = xTaskGetTickCount() - s_press_start;
            if (!power_on && elapsed >= s_on_press_ticks) {
                hal_input_power_set_enable(true);
                power_on = true;
                s_press_start = 0;

                log_psram_size("power_on");

                pwm_motor_shake(500, MOTOR_INTENSITY_5);
                tone_play_and_wait(player, MusicId::Startup, scan_delay);
            } else if (power_on && elapsed >= s_off_press_ticks) {
                log_psram_size("power_off");
                pwm_motor_shake(500, MOTOR_INTENSITY_5);
                tone_play_and_wait(player, MusicId::Shutdown, scan_delay);
                hal_input_power_set_enable(false);
                power_on = false;
                s_press_start = 0;
            }
        }

        player.Update((uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS);
        vTaskDelay(scan_delay);
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

    s_on_press_ticks = pdMS_TO_TICKS(cfg->power_on_long_press_ms);
    s_off_press_ticks = pdMS_TO_TICKS(cfg->power_off_long_press_ms);

    s_evt_queue = xQueueCreate(8, sizeof(encoder_event_t));
    if (s_evt_queue == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    message_bus_subscribe(MSG_TOPIC_ENCODER, on_encoder_event, nullptr);

    s_started = true;
    xTaskCreate(power_task, "power_service", 3072, NULL, 5, &s_task);
    return ESP_OK;
}
