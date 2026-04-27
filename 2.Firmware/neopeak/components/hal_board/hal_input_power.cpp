#include "hal_input_power.h"

// Board-specific implementation: replace GPIO wiring/init here when porting to a new board.

#include "driver/gpio.h"

namespace {
bool s_inited = false;
int s_power_en_pin = -1;
int s_charge_pin = -1;
} // namespace

esp_err_t hal_input_power_init(const hal_input_power_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_power_en_pin = cfg->power_en_pin;
    s_charge_pin = cfg->charge_det_pin;

    // Power enable output.
    gpio_config_t power_cfg = {
        .pin_bit_mask = 1ULL << s_power_en_pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&power_cfg);
    gpio_set_level((gpio_num_t)s_power_en_pin, 1); // Latch power immediately on battery.

    // Charge detect input.
    gpio_config_t chg_cfg = {
        .pin_bit_mask = 1ULL << s_charge_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&chg_cfg);

    s_inited = true;
    return ESP_OK;
}

void hal_input_power_set_enable(bool enable)
{
    if (!s_inited) {
        return;
    }

    gpio_set_level((gpio_num_t)s_power_en_pin, enable ? 1 : 0);
}

int hal_input_power_get_charge_level(void)
{
    if (!s_inited) {
        return -1;
    }

    return gpio_get_level((gpio_num_t)s_charge_pin);
}
