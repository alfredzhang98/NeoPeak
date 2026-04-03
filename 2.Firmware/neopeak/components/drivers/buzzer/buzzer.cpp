#include "buzzer.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// 使用 LEDC 硬件 PWM 产生蜂鸣器的可调频率方波
#include "hal_pwm.h"

static bool s_enabled = true;
static bool s_initialized = false;
static int s_gpio_num = -1;
static ledc_mode_t s_speed_mode = LEDC_LOW_SPEED_MODE;
static ledc_timer_t s_timer = LEDC_TIMER_0;
static ledc_channel_t s_channel = LEDC_CHANNEL_0;
static int32_t s_duration_ms = 0;
static uint32_t s_freq_hz = 0;

static void buzzer_task(void *arg)
{
    (void)arg;
    while (true) {
        if (s_duration_ms > 0) {
            if (s_freq_hz > 0) {
                hal_pwm_set_freq(s_speed_mode, s_timer, s_freq_hz);
                hal_pwm_set_duty(s_speed_mode, s_channel, 128);
                hal_pwm_update_duty(s_speed_mode, s_channel);
            } else {
                hal_pwm_set_duty(s_speed_mode, s_channel, 0);
                hal_pwm_update_duty(s_speed_mode, s_channel);
            }

            vTaskDelay(pdMS_TO_TICKS(s_duration_ms));

            hal_pwm_set_duty(s_speed_mode, s_channel, 0);
            hal_pwm_update_duty(s_speed_mode, s_channel);

            s_duration_ms = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void buzzer_init(const buzzer_config_t *cfg)
{
    if (cfg == nullptr) {
        return;
    }

    s_enabled = cfg->enabled_default;
    s_gpio_num = cfg->gpio_num;
    s_speed_mode = cfg->speed_mode;
    s_timer = cfg->timer_num;
    s_channel = cfg->channel;

    hal_pwm_timer_cfg_t timer_cfg = {
        .speed_mode = cfg->speed_mode,
        .timer_num = cfg->timer_num,
        .duty_resolution = cfg->duty_resolution,
        .freq_hz = cfg->base_freq_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    hal_pwm_timer_init(&timer_cfg);

    hal_pwm_channel_cfg_t ch_cfg = {
        .gpio_num = s_gpio_num,
        .speed_mode = cfg->speed_mode,
        .channel = cfg->channel,
        .timer_sel = cfg->timer_num,
        .duty = 0,
        .hpoint = 0,
    };
    hal_pwm_channel_init(&ch_cfg);

    s_initialized = true;
    xTaskCreate(buzzer_task, "buzzer_task", 2048, NULL, 4, NULL);
}

void buzzer_set_enable(bool en)
{
    s_enabled = en;
    if (!s_initialized) {
        return;
    }
    if (!s_enabled) {
        hal_pwm_set_duty(s_speed_mode, s_channel, 0);
        hal_pwm_update_duty(s_speed_mode, s_channel);
    }
}

void buzzer_tone(uint32_t freq_hz, int32_t duration_ms)
{
    if (!s_enabled || !s_initialized) {
        return;
    }

    if (duration_ms == 0) {
        if (freq_hz > 0) {
            hal_pwm_set_freq(s_speed_mode, s_timer, freq_hz);
            hal_pwm_set_duty(s_speed_mode, s_channel, 128);
            hal_pwm_update_duty(s_speed_mode, s_channel);
        } else {
            hal_pwm_set_duty(s_speed_mode, s_channel, 0);
            hal_pwm_update_duty(s_speed_mode, s_channel);
        }
    } else {
        s_freq_hz = freq_hz;
        s_duration_ms = duration_ms;
    }
}
