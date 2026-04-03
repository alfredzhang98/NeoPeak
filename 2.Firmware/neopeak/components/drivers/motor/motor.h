#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/ledc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MOTOR_INTENSITY_1 = 1,
    MOTOR_INTENSITY_2 = 2,
    MOTOR_INTENSITY_3 = 3,
    MOTOR_INTENSITY_4 = 4,
    MOTOR_INTENSITY_5 = 5,
    MOTOR_INTENSITY_6 = 6,
    MOTOR_INTENSITY_7 = 7,
    MOTOR_INTENSITY_8 = 8,
    MOTOR_INTENSITY_9 = 9,
    MOTOR_INTENSITY_10 = 10,
} motor_intensity_t;

typedef struct {
    int gpio_num;
    ledc_mode_t speed_mode;
    ledc_timer_t timer_num;
    ledc_channel_t channel;
    ledc_timer_bit_t duty_resolution;
    uint32_t pwm_freq_hz;
} motor_pwm_config_t;

void pwm_motor_init(const motor_pwm_config_t *cfg);
void pwm_motor_shake(int32_t duration_ms, motor_intensity_t intensity);

#ifdef __cplusplus
}
#endif
