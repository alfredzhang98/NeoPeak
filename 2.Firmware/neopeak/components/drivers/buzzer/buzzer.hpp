#pragma once

#include <cstdint>

#include "driver/ledc.h"

typedef struct {
	int gpio_num;
	ledc_mode_t speed_mode;
	ledc_timer_t timer_num;
	ledc_channel_t channel;
	ledc_timer_bit_t duty_resolution;
	uint32_t base_freq_hz;
	bool enabled_default;
} buzzer_config_t;

void buzzer_init(const buzzer_config_t *cfg);
void buzzer_set_enable(bool en);
void buzzer_set_volume_percent(uint8_t percent);
void buzzer_tone(uint32_t freq_hz, int32_t duration_ms);
