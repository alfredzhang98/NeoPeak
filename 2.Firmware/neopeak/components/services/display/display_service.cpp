#include "display_service.h"

#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "sdkconfig.h"

#include "encoder_service.h"
#include "feedback_service.h"
#include "hal_pwm.h"
#include "imu_service.h"
#include "message_bus.h"
#include "ui_app.h"

namespace {
constexpr const char *kTag = "display";
constexpr spi_host_device_t kLcdSpiHost = SPI2_HOST;

bool s_started = false;
bool s_ui_ready = false;
esp_lcd_panel_io_handle_t s_io = nullptr;
esp_lcd_panel_handle_t s_panel = nullptr;
lv_display_t *s_disp = nullptr;
bool s_backlight_pwm_ready = false;

constexpr ledc_mode_t kBacklightLedcMode = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t kBacklightLedcTimer = LEDC_TIMER_3;
constexpr ledc_channel_t kBacklightLedcChannel = LEDC_CHANNEL_3;
constexpr ledc_timer_bit_t kBacklightDutyResolution = LEDC_TIMER_10_BIT;
constexpr uint32_t kBacklightPwmHz = 5000;

uint32_t backlight_duty_from_percent(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    const uint32_t max_duty = (1u << static_cast<uint32_t>(kBacklightDutyResolution)) - 1u;
    return (max_duty * percent) / 100u;
}

void init_backlight_pwm()
{
#if CONFIG_SCREEN_BLK_PIN >= 0
    if (s_backlight_pwm_ready) {
        return;
    }

    hal_pwm_timer_cfg_t timer_cfg = {
        .speed_mode = kBacklightLedcMode,
        .timer_num = kBacklightLedcTimer,
        .duty_resolution = kBacklightDutyResolution,
        .freq_hz = kBacklightPwmHz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(hal_pwm_timer_init(&timer_cfg));

    hal_pwm_channel_cfg_t channel_cfg = {
        .gpio_num = CONFIG_SCREEN_BLK_PIN,
        .speed_mode = kBacklightLedcMode,
        .channel = kBacklightLedcChannel,
        .timer_sel = kBacklightLedcTimer,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(hal_pwm_channel_init(&channel_cfg));
    s_backlight_pwm_ready = true;
#endif
}

void apply_backlight_percent(uint8_t percent)
{
#if CONFIG_SCREEN_BLK_PIN >= 0
    init_backlight_pwm();
    ESP_ERROR_CHECK(hal_pwm_set_duty(kBacklightLedcMode, kBacklightLedcChannel, backlight_duty_from_percent(percent)));
    ESP_ERROR_CHECK(hal_pwm_update_duty(kBacklightLedcMode, kBacklightLedcChannel));
#else
    (void)percent;
#endif
}

esp_err_t init_lcd_panel()
{
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = CONFIG_SCREEN_MOSI_PIN;
    bus_cfg.miso_io_num = CONFIG_SCREEN_MISO_PIN;
    bus_cfg.sclk_io_num = CONFIG_SCREEN_SCK_PIN;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.data4_io_num = -1;
    bus_cfg.data5_io_num = -1;
    bus_cfg.data6_io_num = -1;
    bus_cfg.data7_io_num = -1;
    bus_cfg.max_transfer_sz = CONFIG_SCREEN_HOR_RES * 80 * sizeof(uint16_t);
    bus_cfg.flags = SPICOMMON_BUSFLAG_MASTER;
    bus_cfg.isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO;
    bus_cfg.intr_flags = 0;

    esp_err_t err = spi_bus_initialize(kLcdSpiHost, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(kTag, "spi bus init failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.cs_gpio_num = static_cast<gpio_num_t>(CONFIG_SCREEN_CS_PIN);
    io_cfg.dc_gpio_num = static_cast<gpio_num_t>(CONFIG_SCREEN_DC_PIN);
    io_cfg.spi_mode = 3;  // ST7789 typically uses SPI mode 3 (CPOL=1, CPHA=1)
    io_cfg.pclk_hz = CONFIG_SCREEN_SPI_HZ;
    io_cfg.trans_queue_depth = 10;
    io_cfg.on_color_trans_done = nullptr;
    io_cfg.user_ctx = nullptr;
    io_cfg.lcd_cmd_bits = 8;
    io_cfg.lcd_param_bits = 8;
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)kLcdSpiHost, &io_cfg, &s_io);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "panel io init failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_lcd_panel_dev_config_t panel_cfg = {};
    panel_cfg.reset_gpio_num = static_cast<gpio_num_t>(CONFIG_SCREEN_RST_PIN);
    panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    panel_cfg.bits_per_pixel = 16;
    panel_cfg.vendor_config = nullptr;
    err = esp_lcd_new_panel_st7789(s_io, &panel_cfg, &s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "st7789 init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, CONFIG_SCREEN_INVERT_COLOR));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_panel, CONFIG_SCREEN_X_GAP, CONFIG_SCREEN_Y_GAP));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));
    apply_backlight_percent(80);
    return ESP_OK;
}

esp_err_t init_lvgl()
{
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    esp_err_t err = lvgl_port_init(&lvgl_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "lvgl port init failed: %s", esp_err_to_name(err));
        return err;
    }

    lvgl_port_display_cfg_t disp_cfg = {};
    disp_cfg.io_handle = s_io;
    disp_cfg.panel_handle = s_panel;
    disp_cfg.control_handle = nullptr;
    disp_cfg.buffer_size = CONFIG_SCREEN_BUFFER_SIZE;
    disp_cfg.double_buffer = true;
    disp_cfg.trans_size = 0;
    disp_cfg.hres = CONFIG_SCREEN_HOR_RES;
    disp_cfg.vres = CONFIG_SCREEN_VER_RES;
    disp_cfg.monochrome = false;
    disp_cfg.rotation.swap_xy = false;
    disp_cfg.rotation.mirror_x = false;
    disp_cfg.rotation.mirror_y = false;
    disp_cfg.color_format = LV_COLOR_FORMAT_RGB565;
    disp_cfg.flags.buff_dma = true;
    disp_cfg.flags.buff_spiram = false;
    disp_cfg.flags.sw_rotate = false;
    disp_cfg.flags.swap_bytes = true;
    disp_cfg.flags.full_refresh = false;
    disp_cfg.flags.direct_mode = false;

    s_disp = lvgl_port_add_disp(&disp_cfg);
    return (s_disp != nullptr) ? ESP_OK : ESP_FAIL;
}

void on_imu_msg(const message_t *msg, void *user_ctx)
{
    (void)user_ctx;
    if (msg == nullptr || msg->data == nullptr || msg->size < sizeof(imu_sample_t) || !s_ui_ready) {
        return;
    }

    const imu_sample_t *sample = (const imu_sample_t *)msg->data;
    if (lvgl_port_lock(0)) {
        ui_app_handle_imu(sample->attitude.q,
                          sample->attitude.calib_percent,
                          sample->attitude.gyro_bias_rad_s);
        lvgl_port_unlock();
    }
}

void on_encoder_msg(const message_t *msg, void *user_ctx)
{
    (void)user_ctx;
    if (msg == nullptr || msg->data == nullptr || msg->size < sizeof(encoder_event_t) || !s_ui_ready) {
        return;
    }

    const encoder_event_t *evt = (const encoder_event_t *)msg->data;
    feedback_haptic_t haptic = FEEDBACK_HAPTIC_NONE;

    if (lvgl_port_lock(0)) {
        if (evt->type == ENCODER_EVENT_ROTATE) {
            ui_app_handle_encoder_rotate(evt->delta);
            haptic = FEEDBACK_HAPTIC_LIGHT;
        } else if (evt->type == ENCODER_EVENT_BUTTON) {
            ui_app_handle_encoder_button(evt->pressed, msg->timestamp_ms);
            if (!evt->pressed) {
                haptic = FEEDBACK_HAPTIC_MEDIUM;
            }
        }
        lvgl_port_unlock();
    }

    if (haptic != FEEDBACK_HAPTIC_NONE) {
        feedback_post(FEEDBACK_MUSIC_NONE, haptic);
    }
}

void on_ui_settings_changed(const ui_app_settings_t *settings, void *user_ctx)
{
    (void)user_ctx;
    if (settings == nullptr) {
        return;
    }

    apply_backlight_percent(settings->backlight_percent);
    feedback_service_set_volume_percent(settings->volume_percent);
    feedback_service_set_haptic_percent(settings->haptic_percent);
}

void on_ui_imu_active_changed(bool active, void *user_ctx)
{
    (void)user_ctx;
    imu_service_set_enabled(active);
}
} // namespace

esp_err_t display_service_start(void)
{
    if (s_started) {
        return ESP_OK;
    }

    esp_err_t err = init_lcd_panel();
    if (err != ESP_OK) {
        return err;
    }

    err = init_lvgl();
    if (err != ESP_OK) {
        return err;
    }

    if (lvgl_port_lock(0)) {
        ui_app_config_t ui_cfg = {};
        ui_cfg.settings_changed_cb = on_ui_settings_changed;
        ui_cfg.imu_active_changed_cb = on_ui_imu_active_changed;
        ui_cfg.user_ctx = nullptr;
        ui_app_create(lv_screen_active(), &ui_cfg);
        s_ui_ready = true;
        lvgl_port_unlock();
    }

    message_bus_subscribe(MSG_TOPIC_IMU, on_imu_msg, nullptr);
    message_bus_subscribe(MSG_TOPIC_ENCODER, on_encoder_msg, nullptr);
    s_started = true;
    ESP_LOGI(kTag, "started st7789 %dx%d", CONFIG_SCREEN_HOR_RES, CONFIG_SCREEN_VER_RES);
    return ESP_OK;
}

void display_service_blank(void)
{
    apply_backlight_percent(0);
    if (s_panel != nullptr) {
        esp_lcd_panel_disp_on_off(s_panel, false);
    }
}
