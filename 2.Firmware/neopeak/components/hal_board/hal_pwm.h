#pragma once

#include <stdint.h>

#include "driver/ledc.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ledc_mode_t speed_mode;
    ledc_timer_t timer_num;
    ledc_timer_bit_t duty_resolution;
    uint32_t freq_hz;
    ledc_clk_cfg_t clk_cfg;
} hal_pwm_timer_cfg_t;

typedef struct {
    int gpio_num;
    ledc_mode_t speed_mode;
    ledc_channel_t channel;
    ledc_timer_t timer_sel;
    uint32_t duty;
    int hpoint;
} hal_pwm_channel_cfg_t;

esp_err_t hal_pwm_timer_init(const hal_pwm_timer_cfg_t *cfg);

esp_err_t hal_pwm_channel_init(const hal_pwm_channel_cfg_t *cfg);

esp_err_t hal_pwm_set_freq(ledc_mode_t mode, ledc_timer_t timer, uint32_t freq_hz);

esp_err_t hal_pwm_set_duty(ledc_mode_t mode, ledc_channel_t channel, uint32_t duty);

esp_err_t hal_pwm_update_duty(ledc_mode_t mode, ledc_channel_t channel);

#ifdef __cplusplus
}
#endif
