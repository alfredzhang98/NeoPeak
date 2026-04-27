#include "encoder_service.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hal_encoder.h"
#include "message_bus.h"

namespace {
TaskHandle_t s_task = nullptr;
bool s_started = false;
uint32_t s_poll_ms = 5;
uint32_t s_debounce_ms = 30;
bool s_button_active_low = true;

int s_last_state = 0;
int s_button_state = 0;
TickType_t s_button_change_tick = 0;

const int8_t kDeltaTable[16] = {
    0,  1, -1,  0,
   -1,  0,  0,  1,
    1,  0,  0, -1,
    0, -1,  1,  0
};

int read_state()
{
    int a = hal_encoder_get_a_level();
    int b = hal_encoder_get_b_level();
    if (a < 0 || b < 0) {
        return -1;
    }
    return (a << 1) | b;
}

void publish_rotate(int8_t delta)
{
    encoder_event_t evt = {
        .type = ENCODER_EVENT_ROTATE,
        .delta = delta,
        .pressed = false,
    };
    message_bus_publish(MSG_TOPIC_ENCODER, &evt, sizeof(evt));
}

void publish_button(bool pressed)
{
    encoder_event_t evt = {
        .type = ENCODER_EVENT_BUTTON,
        .delta = 0,
        .pressed = pressed,
    };
    message_bus_publish(MSG_TOPIC_ENCODER, &evt, sizeof(evt));
}

void encoder_task(void *arg)
{
    (void)arg;

    s_last_state = read_state();
    s_button_state = hal_encoder_get_button_level();
    s_button_change_tick = xTaskGetTickCount();

    while (true) {
        TickType_t delay_ticks = pdMS_TO_TICKS(s_poll_ms);
        if (delay_ticks == 0) {
            delay_ticks = 1;
        }

        int state = read_state();
        if (state >= 0 && s_last_state >= 0 && state != s_last_state) {
            int idx = (s_last_state << 2) | state;
            int8_t delta = kDeltaTable[idx & 0x0F];
            if (delta != 0) {
                static int8_t s_accum = 0;
                s_accum += delta;
                while (s_accum >= 4) {
                    publish_rotate(1);
                    s_accum -= 4;
                }
                while (s_accum <= -4) {
                    publish_rotate(-1);
                    s_accum += 4;
                }
            }
            s_last_state = state;
        }

        int button_level = hal_encoder_get_button_level();
        if (button_level >= 0 && button_level != s_button_state) {
            s_button_state = button_level;
            s_button_change_tick = xTaskGetTickCount();
        }

        if (button_level >= 0) {
            TickType_t now = xTaskGetTickCount();
            TickType_t elapsed = now - s_button_change_tick;
            if (elapsed >= pdMS_TO_TICKS(s_debounce_ms)) {
                bool pressed = s_button_active_low ? (button_level == 0) : (button_level != 0);
                static bool last_pressed = false;
                if (pressed != last_pressed) {
                    last_pressed = pressed;
                    publish_button(pressed);
                }
            }
        }

        vTaskDelay(delay_ticks);
    }
}
} // namespace

esp_err_t encoder_service_start(const encoder_service_config_t *cfg)
{
    if (s_started) {
        return ESP_OK;
    }

    if (cfg == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    const hal_encoder_config_t enc_cfg = {
        .pin_a = cfg->pin_a,
        .pin_b = cfg->pin_b,
        .pin_button = cfg->pin_button,
        .button_active_low = cfg->button_active_low,
    };

    esp_err_t err = hal_encoder_init(&enc_cfg);
    if (err != ESP_OK) {
        return err;
    }

    s_poll_ms = (cfg->poll_ms == 0) ? 1 : cfg->poll_ms;
    s_debounce_ms = cfg->debounce_ms;
    s_button_active_low = cfg->button_active_low;

    s_started = true;
    xTaskCreate(encoder_task, "encoder_service", 3072, NULL, 5, &s_task);
    return ESP_OK;
}

bool encoder_service_is_button_pressed(void)
{
    int level = hal_encoder_get_button_level();
    if (level < 0) {
        return false;
    }
    return s_button_active_low ? (level == 0) : (level != 0);
}
