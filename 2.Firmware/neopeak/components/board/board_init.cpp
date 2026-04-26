#include "board_init.h"

#include "esp_log.h"
#include "esp_psram.h"
#include "sdkconfig.h"

#include "buzzer.hpp"
#include "encoder_service.h"
#include "feedback_service.h"
#include "message_bus.h"
#include "motor.h"
#include "power_service.h"

namespace {
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
} // namespace

void board_init(void)
{
    log_psram_size("boot");

    message_bus_init();

    const buzzer_config_t buzzer_cfg = {
        .gpio_num = CONFIG_BUZZ_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = (ledc_timer_t)CONFIG_BUZZ_CHANNEL,
        .channel = (ledc_channel_t)CONFIG_BUZZ_CHANNEL,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .base_freq_hz = 2000,
        .enabled_default = CONFIG_SOUND_ENABLE_DEFAULT,
    };
    buzzer_init(&buzzer_cfg);

    const motor_pwm_config_t motor_cfg = {
        .gpio_num = CONFIG_MOTOR_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_1,
        .channel = LEDC_CHANNEL_1,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .pwm_freq_hz = 200,
    };
    pwm_motor_init(&motor_cfg);

    const encoder_service_config_t enc_cfg = {
        .pin_a = CONFIG_ENCODER_A_PIN,
        .pin_b = CONFIG_ENCODER_B_PIN,
        .pin_button = CONFIG_ENCODER_PUSH_PIN,
        .button_active_low = CONFIG_POWER_BUTTON_ACTIVE_LOW,
        .poll_ms = 5,
        .debounce_ms = 30,
    };
    encoder_service_start(&enc_cfg);

    feedback_service_start();

    const power_service_config_t power_cfg = {
        .io = {
            .power_en_pin = CONFIG_POWER_EN_PIN,
            .charge_det_pin = CONFIG_BAT_CHG_DET_PIN,
        },
        .power_off_long_press_ms = CONFIG_POWER_OFF_LONG_PRESS_MS,
    };
    power_service_start(&power_cfg);
}
