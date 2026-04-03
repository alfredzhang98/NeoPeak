#include "hal_pwm.h"

// Board-specific implementation: replace PWM setup here when porting to a new board.

esp_err_t hal_pwm_timer_init(const hal_pwm_timer_cfg_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // PWM 定时器配置
    ledc_timer_config_t timer_cfg = {
        .speed_mode = cfg->speed_mode,
        .duty_resolution = cfg->duty_resolution,
        .timer_num = cfg->timer_num,
        .freq_hz = cfg->freq_hz,
        .clk_cfg = cfg->clk_cfg,
        .deconfigure = false,
    };
    return ledc_timer_config(&timer_cfg);
}

esp_err_t hal_pwm_channel_init(const hal_pwm_channel_cfg_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // PWM 通道配置
    ledc_channel_config_t ch_cfg = {
        .gpio_num = cfg->gpio_num,
        .speed_mode = cfg->speed_mode,
        .channel = cfg->channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = cfg->timer_sel,
        .duty = cfg->duty,
        .hpoint = cfg->hpoint,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = { .output_invert = 0 },
    };

    return ledc_channel_config(&ch_cfg);
}

esp_err_t hal_pwm_set_freq(ledc_mode_t mode, ledc_timer_t timer, uint32_t freq_hz)
{
    return ledc_set_freq(mode, timer, freq_hz);
}

esp_err_t hal_pwm_set_duty(ledc_mode_t mode, ledc_channel_t channel, uint32_t duty)
{
    return ledc_set_duty(mode, channel, duty);
}

esp_err_t hal_pwm_update_duty(ledc_mode_t mode, ledc_channel_t channel)
{
    return ledc_update_duty(mode, channel);
}
