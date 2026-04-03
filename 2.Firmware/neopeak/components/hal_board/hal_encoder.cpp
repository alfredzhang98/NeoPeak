#include "hal_encoder.h"

// Board-specific implementation: replace GPIO wiring/init here when porting to a new board.

#include "driver/gpio.h"

namespace {
bool s_inited = false;
int s_pin_a = -1;
int s_pin_b = -1;
int s_pin_button = -1;
bool s_button_active_low = true;
} // namespace

esp_err_t hal_encoder_init(const hal_encoder_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_pin_a = cfg->pin_a;
    s_pin_b = cfg->pin_b;
    s_pin_button = cfg->pin_button;
    s_button_active_low = cfg->button_active_low;

    // 编码器 A/B 相输入
    gpio_config_t encoder_cfg = {
        .pin_bit_mask = (1ULL << s_pin_a) | (1ULL << s_pin_b),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&encoder_cfg);

    // 编码器按键输入，上拉/下拉按有效电平设置
    gpio_config_t button_cfg = {
        .pin_bit_mask = 1ULL << s_pin_button,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = s_button_active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = s_button_active_low ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&button_cfg);

    s_inited = true;
    return ESP_OK;
}

int hal_encoder_get_a_level(void)
{
    if (!s_inited) {
        return -1;
    }

    return gpio_get_level((gpio_num_t)s_pin_a);
}

int hal_encoder_get_b_level(void)
{
    if (!s_inited) {
        return -1;
    }

    return gpio_get_level((gpio_num_t)s_pin_b);
}

int hal_encoder_get_button_level(void)
{
    if (!s_inited) {
        return -1;
    }

    return gpio_get_level((gpio_num_t)s_pin_button);
}

bool hal_encoder_is_button_pressed(void)
{
    int level = hal_encoder_get_button_level();
    if (level < 0) {
        return false;
    }

    return s_button_active_low ? (level == 0) : (level != 0);
}
