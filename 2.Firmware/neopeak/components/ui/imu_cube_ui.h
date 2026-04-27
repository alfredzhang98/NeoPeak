#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void imu_cube_ui_create(lv_obj_t *root);

// Drive the cube + calibration overlay from a freshly received IMU sample.
// calib_percent: 0..100, where 100 means the gyro-bias calibration finished.
// gyro_bias_rad_s: per-axis bias used for compensation (valid once calib_percent == 100).
void imu_cube_ui_update(const float q[4], uint8_t calib_percent,
                        const float gyro_bias_rad_s[3]);

// Snap the cube's "zero" to the current attitude (only effective in cube view).
void imu_cube_ui_reset(void);

// If the calibration overlay is showing the "done" prompt, consume the click
// to dismiss the overlay and return true. Otherwise return false so the click
// propagates to the regular cube-page logic (reset cube / pop page).
bool imu_cube_ui_consume_click(void);

#ifdef __cplusplus
}
#endif
