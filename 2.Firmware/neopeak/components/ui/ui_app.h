#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t backlight_percent;
    uint8_t volume_percent;
    uint8_t haptic_percent;
    bool    yaw_enabled;       // when false, IMU cube ignores yaw (locks forward)
} ui_app_settings_t;

typedef void (*ui_app_settings_changed_cb_t)(const ui_app_settings_t *settings, void *user_ctx);
typedef void (*ui_app_imu_active_changed_cb_t)(bool active, void *user_ctx);

typedef struct {
    ui_app_settings_changed_cb_t settings_changed_cb;
    ui_app_imu_active_changed_cb_t imu_active_changed_cb;
    void *user_ctx;
} ui_app_config_t;

void ui_app_create(lv_obj_t *root, const ui_app_config_t *cfg);
void ui_app_handle_encoder_rotate(int8_t delta);
void ui_app_handle_encoder_button(bool pressed, uint32_t timestamp_ms);
void ui_app_handle_imu(const float q[4], uint8_t calib_percent,
                       const float gyro_bias_rad_s[3]);

#ifdef __cplusplus
}
#endif
