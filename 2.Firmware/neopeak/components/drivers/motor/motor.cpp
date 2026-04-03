#include "motor.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal_pwm.h"

static bool s_connected = true;
static bool s_initialized = false;
static int32_t s_duration_ms = 0;
static uint32_t s_duty = 0;
static int s_gpio_num = -1;
static ledc_mode_t s_speed_mode = LEDC_LOW_SPEED_MODE;
static ledc_timer_t s_timer = LEDC_TIMER_1;
static ledc_channel_t s_channel = LEDC_CHANNEL_1;
static ledc_timer_bit_t s_duty_resolution = LEDC_TIMER_10_BIT;
static uint32_t s_pwm_freq_hz = 200;

static uint32_t motor_level_to_duty(motor_intensity_t intensity)
{
    const uint32_t max_duty = (1u << s_duty_resolution) - 1u;
    if ((int)intensity <= 0) {
        return 0;
    }
    if ((int)intensity > 10) {
        intensity = MOTOR_INTENSITY_10;
    }

    return (max_duty * (uint32_t)intensity) / 10u;
}

static void motor_task(void *arg)
{
    (void)arg;
    while (true) {
        if (s_duration_ms > 0) {
            if (s_duty > 0) {
                hal_pwm_set_duty(s_speed_mode, s_channel, s_duty);
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

void pwm_motor_init(const motor_pwm_config_t *cfg)
{
    if (cfg == nullptr) {
        return;
    }

    s_gpio_num = cfg->gpio_num;
    s_speed_mode = cfg->speed_mode;
    s_timer = cfg->timer_num;
    s_channel = cfg->channel;
    s_duty_resolution = cfg->duty_resolution;
    s_pwm_freq_hz = cfg->pwm_freq_hz;

    if (s_gpio_num < 0) {
        s_connected = false;
        return;
    }

    hal_pwm_timer_cfg_t timer_cfg = {
        .speed_mode = s_speed_mode,
        .timer_num = s_timer,
        .duty_resolution = s_duty_resolution,
        .freq_hz = s_pwm_freq_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    hal_pwm_timer_init(&timer_cfg);

    hal_pwm_channel_cfg_t ch_cfg = {
        .gpio_num = s_gpio_num,
        .speed_mode = s_speed_mode,
        .channel = s_channel,
        .timer_sel = s_timer,
        .duty = 0,
        .hpoint = 0,
    };
    hal_pwm_channel_init(&ch_cfg);

    s_initialized = true;
    xTaskCreate(motor_task, "motor_task", 2048, NULL, 4, NULL);
}

void pwm_motor_shake(int32_t duration_ms, motor_intensity_t intensity)
{
    if (!s_connected || !s_initialized) {
        return;
    }

    if (duration_ms <= 0) {
        s_duration_ms = 0;
        s_duty = 0;
        hal_pwm_set_duty(s_speed_mode, s_channel, 0);
        hal_pwm_update_duty(s_speed_mode, s_channel);
        return;
    }

    s_duty = motor_level_to_duty(intensity);
    s_duration_ms = duration_ms;
}
