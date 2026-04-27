#include "imu_service.h"

#include <math.h>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hal_i2c.h"
#include "message_bus.h"

namespace {
constexpr const char *kTag = "imu_service";
constexpr i2c_port_t kI2cPort = I2C_NUM_0;
constexpr uint32_t kDefaultI2cHz = 400000;
constexpr uint32_t kDefaultSampleMs = 10;
constexpr float kPi = 3.1415926535f;
constexpr float kDegToRad = 0.0174532925f;

// Madgwick filter gain. Larger = trust accel more (faster convergence, more
// jitter from accel noise). 0.04 is the paper's default; ICM-42688-P has very
// low gyro noise so we can drop to 0.015 — gyro carries the high-frequency
// motion, accel only needs to gently hold long-term gravity reference.
// Initial pose is seeded from accel mean during boot so we don't need a high
// beta for fast convergence.
constexpr float kMadgwickBeta = 0.015f;

// Gyro bias calibration: average N consecutive stationary samples after the
// sensor powers up to subtract per-unit DC offset. ~0.6 s at 100 Hz once the
// watch is held still. The threshold accommodates normal hand tremor; gross
// motion resets the counter so the bias estimate stays clean.
constexpr int kBiasSampleCount = 60;
constexpr float kCalibStillDps = 12.0f;

// === IMU axis remap (empirically calibrated for this PCB mount) ============
// Each row maps a chip body-axis reading to the screen frame used by the cube
// renderer (X right, Y up, Z out of screen).
//
// This board's chip is mounted so that screen-X and screen-Z are anti-aligned
// with the chip's body-X and body-Z (a 180-degree rotation around body-Y).
// The same sign pattern must be applied to accel and gyro because that
// composes to a single proper rotation; mixing signs across the two would
// produce a reflection (improper rotation) and the filter would diverge.
//
// If the cube rotates the wrong way around any axis after a hardware change,
// flip both the accel and gyro entries on that axis together.
static inline void remap_imu(const icm42688p_data_t *raw,
                             float out_a[3],
                             float out_g_rad[3])
{
    out_a[0] = +raw->accel_g[0];
    out_a[1] = -raw->accel_g[1];
    out_a[2] = -raw->accel_g[2];
    out_g_rad[0] = +raw->gyro_dps[0] * kDegToRad;
    out_g_rad[1] = -raw->gyro_dps[1] * kDegToRad;
    out_g_rad[2] = -raw->gyro_dps[2] * kDegToRad;
}
// ===========================================================================

TaskHandle_t s_task = nullptr;
bool s_started = false;
volatile bool s_enabled = false;
bool s_sensor_powered = false;
hal_i2c_bus_t s_bus = {};
hal_i2c_device_t s_dev = {};
icm42688p_t s_imu = {};
uint32_t s_sample_period_ms = kDefaultSampleMs;
uint32_t s_last_sample_ms = 0;

// Filter state.
float s_q[4] = {1.0f, 0.0f, 0.0f, 0.0f};   // orientation quaternion (w, x, y, z)
float s_gyro_bias[3] = {0.0f, 0.0f, 0.0f}; // estimated gyro DC offset (rad/s)
float s_bias_accum[3] = {0.0f, 0.0f, 0.0f};
float s_accel_accum[3] = {0.0f, 0.0f, 0.0f};
int s_bias_samples = 0;
bool s_bias_ready = false;

TickType_t delay_ticks(uint32_t ms)
{
    TickType_t ticks = pdMS_TO_TICKS(ms);
    return (ticks == 0) ? 1 : ticks;
}

uint32_t now_ms()
{
    return (uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS;
}

void reset_filter_state()
{
    s_last_sample_ms = 0;
    s_q[0] = 1.0f;
    s_q[1] = 0.0f;
    s_q[2] = 0.0f;
    s_q[3] = 0.0f;
    s_gyro_bias[0] = s_gyro_bias[1] = s_gyro_bias[2] = 0.0f;
    s_bias_accum[0] = s_bias_accum[1] = s_bias_accum[2] = 0.0f;
    s_accel_accum[0] = s_accel_accum[1] = s_accel_accum[2] = 0.0f;
    s_bias_samples = 0;
    s_bias_ready = false;
}

// Compute the quaternion that rotates the body so that its measured
// accelerometer direction matches world gravity. At rest, the accel reading
// (in body frame) corresponds to R^T * (0, 0, 1) per the Madgwick reference
// frame, so we solve for q such that R^T(q) * (0, 0, 1) = a_normalized.
// Closed-form via the "shortest arc" between the two unit vectors, with the
// antiparallel case (chip mounted Z-down) handled explicitly.
void initial_q_from_accel(const float a[3], float out_q[4])
{
    float ax = a[0], ay = a[1], az = a[2];
    float n = sqrtf(ax * ax + ay * ay + az * az);
    if (n < 1e-6f) {
        out_q[0] = 1.0f; out_q[1] = 0.0f; out_q[2] = 0.0f; out_q[3] = 0.0f;
        return;
    }
    ax /= n; ay /= n; az /= n;
    // Madgwick world reference: gravity along +Z. We need R^T*(0,0,1) == a.
    // Equivalently, R rotates a to (0,0,1). Use shortest-arc formula:
    //   q = normalize( |a| + dot(a, z), cross(a, z) )
    // where a is the body accel direction and z = (0, 0, 1).
    float dot = az;  // a . (0,0,1)
    if (dot < -1.0f + 1e-4f) {
        // Antiparallel: chip's body Z points opposite to world Z.
        // Pick a 180-degree rotation around the body X axis as a safe choice;
        // user can refine later via the imu_cube_ui_reset offset.
        out_q[0] = 0.0f; out_q[1] = 1.0f; out_q[2] = 0.0f; out_q[3] = 0.0f;
        return;
    }
    // cross(a, z) = (ay, -ax, 0)
    float w = 1.0f + dot;
    float qx =  ay;
    float qy = -ax;
    float qz =  0.0f;
    float qn = sqrtf(w * w + qx * qx + qy * qy + qz * qz);
    if (qn < 1e-9f) {
        out_q[0] = 1.0f; out_q[1] = 0.0f; out_q[2] = 0.0f; out_q[3] = 0.0f;
        return;
    }
    float inv = 1.0f / qn;
    out_q[0] = w  * inv;
    out_q[1] = qx * inv;
    out_q[2] = qy * inv;
    out_q[3] = qz * inv;
}

// Madgwick AHRS update for 6-axis IMU (gyro + accel only, no magnetometer).
// Reference: S. Madgwick, "An efficient orientation filter for inertial and
// inertial/magnetic sensor arrays", 2010, eq. 25.
//
// gx, gy, gz: bias-corrected angular rate, rad/s
// ax, ay, az: accelerometer reading in g (any positive scale, normalized here)
// dt:        seconds since previous call
void madgwick_step(float gx, float gy, float gz,
                   float ax, float ay, float az, float dt)
{
    float q0 = s_q[0], q1 = s_q[1], q2 = s_q[2], q3 = s_q[3];

    // Rate of change of quaternion from gyroscope (pure integration term).
    float qd0 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
    float qd1 = 0.5f * ( q0 * gx + q2 * gz - q3 * gy);
    float qd2 = 0.5f * ( q0 * gy - q1 * gz + q3 * gx);
    float qd3 = 0.5f * ( q0 * gz + q1 * gy - q2 * gx);

    // Apply accel correction only when reading is non-zero (avoid free-fall
    // singularity).
    float anorm_sq = ax * ax + ay * ay + az * az;
    if (anorm_sq > 1e-9f) {
        float inv = 1.0f / sqrtf(anorm_sq);
        ax *= inv; ay *= inv; az *= inv;

        // Gradient of objective function f = R^T(q) * (0,0,1) - (ax,ay,az).
        float f1 = 2.0f * (q1 * q3 - q0 * q2) - ax;
        float f2 = 2.0f * (q0 * q1 + q2 * q3) - ay;
        float f3 = 2.0f * (0.5f - q1 * q1 - q2 * q2) - az;

        float s0 = -2.0f * q2 * f1 + 2.0f * q1 * f2;
        float s1 =  2.0f * q3 * f1 + 2.0f * q0 * f2 - 4.0f * q1 * f3;
        float s2 = -2.0f * q0 * f1 + 2.0f * q3 * f2 - 4.0f * q2 * f3;
        float s3 =  2.0f * q1 * f1 + 2.0f * q2 * f2;

        float snorm_sq = s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3;
        if (snorm_sq > 1e-9f) {
            float inv_s = 1.0f / sqrtf(snorm_sq);
            qd0 -= kMadgwickBeta * s0 * inv_s;
            qd1 -= kMadgwickBeta * s1 * inv_s;
            qd2 -= kMadgwickBeta * s2 * inv_s;
            qd3 -= kMadgwickBeta * s3 * inv_s;
        }
    }

    q0 += qd0 * dt;
    q1 += qd1 * dt;
    q2 += qd2 * dt;
    q3 += qd3 * dt;

    float n_sq = q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3;
    if (n_sq > 1e-9f) {
        float inv = 1.0f / sqrtf(n_sq);
        s_q[0] = q0 * inv;
        s_q[1] = q1 * inv;
        s_q[2] = q2 * inv;
        s_q[3] = q3 * inv;
    }
}

// Extract Euler angles (XYZ intrinsic = yaw-pitch-roll) from quaternion for
// log/UI display. The quaternion remains the source of truth for rendering.
void quat_to_euler(const float q[4], float *roll, float *pitch, float *yaw)
{
    float w = q[0], x = q[1], y = q[2], z = q[3];
    *roll  = atan2f(2.0f * (w * x + y * z), 1.0f - 2.0f * (x * x + y * y));
    float sinp = 2.0f * (w * y - z * x);
    if (sinp > 1.0f)  sinp = 1.0f;
    if (sinp < -1.0f) sinp = -1.0f;
    *pitch = asinf(sinp);
    *yaw   = atan2f(2.0f * (w * z + x * y), 1.0f - 2.0f * (y * y + z * z));
}

void set_sensor_power(bool on)
{
    if (s_sensor_powered == on || !s_imu.initialized) {
        return;
    }

    esp_err_t err = ESP_OK;
    if (on) {
        err = icm42688p_set_power(&s_imu,
                                  ICM42688P_GYRO_MODE_LOW_NOISE,
                                  ICM42688P_ACCEL_MODE_LOW_NOISE,
                                  true);
        reset_filter_state();
    } else {
        err = icm42688p_set_power(&s_imu,
                                  ICM42688P_GYRO_MODE_OFF,
                                  ICM42688P_ACCEL_MODE_OFF,
                                  false);
    }

    if (err == ESP_OK) {
        s_sensor_powered = on;
    } else {
        ESP_LOGW(kTag, "sensor power %s failed: %s", on ? "on" : "off", esp_err_to_name(err));
    }
}

void update_attitude(imu_sample_t *sample)
{
    if (sample == nullptr) {
        return;
    }

    float a[3];        // accel in g
    float g_raw[3];    // gyro in rad/s, before bias correction
    remap_imu(&sample->data, a, g_raw);

    // dt from message bus timestamps (handles jitter); fall back to nominal.
    uint32_t timestamp = sample->timestamp_ms;
    float dt = (s_last_sample_ms == 0 || timestamp <= s_last_sample_ms)
                   ? ((float)s_sample_period_ms * 0.001f)
                   : ((float)(timestamp - s_last_sample_ms) * 0.001f);
    if (dt <= 0.0f || dt > 0.5f) {
        dt = (float)s_sample_period_ms * 0.001f;
    }
    s_last_sample_ms = timestamp;

    // Stationary gyro bias estimation during the first ~1 s after power-on.
    // The user is implicitly expected to keep the watch still; if motion is
    // detected we drop those samples but keep waiting (don't poison bias).
    if (!s_bias_ready) {
        // Require N consecutive stationary samples — any motion above the
        // threshold resets the accumulator. Combined with a UI overlay
        // telling the user "hold still", this gives a clean bias estimate
        // in roughly 0.6 s and resists sloppy handling.
        float gmag_dps = sqrtf(g_raw[0] * g_raw[0] + g_raw[1] * g_raw[1] +
                               g_raw[2] * g_raw[2]) / kDegToRad;
        if (gmag_dps < kCalibStillDps) {
            s_bias_accum[0] += g_raw[0];
            s_bias_accum[1] += g_raw[1];
            s_bias_accum[2] += g_raw[2];
            s_accel_accum[0] += a[0];
            s_accel_accum[1] += a[1];
            s_accel_accum[2] += a[2];
            s_bias_samples++;
            if (s_bias_samples >= kBiasSampleCount) {
                float inv_n = 1.0f / (float)s_bias_samples;
                s_gyro_bias[0] = s_bias_accum[0] * inv_n;
                s_gyro_bias[1] = s_bias_accum[1] * inv_n;
                s_gyro_bias[2] = s_bias_accum[2] * inv_n;
                float a_mean[3] = {
                    s_accel_accum[0] * inv_n,
                    s_accel_accum[1] * inv_n,
                    s_accel_accum[2] * inv_n,
                };
                initial_q_from_accel(a_mean, s_q);
                s_bias_ready = true;
                ESP_LOGI(kTag, "gyro bias rad/s: %.5f %.5f %.5f",
                         s_gyro_bias[0], s_gyro_bias[1], s_gyro_bias[2]);
                ESP_LOGI(kTag, "init q: %.3f %.3f %.3f %.3f  (accel mean g: %.2f %.2f %.2f)",
                         s_q[0], s_q[1], s_q[2], s_q[3],
                         a_mean[0], a_mean[1], a_mean[2]);
            }
        } else {
            // Motion detected — start over so we don't fold transient
            // angular rate into the DC bias estimate.
            s_bias_samples = 0;
            s_bias_accum[0] = s_bias_accum[1] = s_bias_accum[2] = 0.0f;
            s_accel_accum[0] = s_accel_accum[1] = s_accel_accum[2] = 0.0f;
        }
        // Surface progress + identity attitude while calibrating.
        sample->attitude.q[0] = 1.0f;
        sample->attitude.q[1] = 0.0f;
        sample->attitude.q[2] = 0.0f;
        sample->attitude.q[3] = 0.0f;
        sample->attitude.roll_rad = 0.0f;
        sample->attitude.pitch_rad = 0.0f;
        sample->attitude.yaw_rad = 0.0f;
        sample->attitude.calib_percent =
            (uint8_t)((s_bias_samples * 100) / kBiasSampleCount);
        sample->attitude.gyro_bias_rad_s[0] = 0.0f;
        sample->attitude.gyro_bias_rad_s[1] = 0.0f;
        sample->attitude.gyro_bias_rad_s[2] = 0.0f;
        return;
    }

    float gx = g_raw[0] - s_gyro_bias[0];
    float gy = g_raw[1] - s_gyro_bias[1];
    float gz = g_raw[2] - s_gyro_bias[2];

    madgwick_step(gx, gy, gz, a[0], a[1], a[2], dt);

    sample->attitude.q[0] = s_q[0];
    sample->attitude.q[1] = s_q[1];
    sample->attitude.q[2] = s_q[2];
    sample->attitude.q[3] = s_q[3];
    quat_to_euler(s_q,
                  &sample->attitude.roll_rad,
                  &sample->attitude.pitch_rad,
                  &sample->attitude.yaw_rad);
    sample->attitude.calib_percent = 100;
    sample->attitude.gyro_bias_rad_s[0] = s_gyro_bias[0];
    sample->attitude.gyro_bias_rad_s[1] = s_gyro_bias[1];
    sample->attitude.gyro_bias_rad_s[2] = s_gyro_bias[2];
}

void imu_task(void *arg)
{
    (void)arg;

    while (true) {
        bool should_enable = s_enabled;
        if (should_enable != s_sensor_powered) {
            set_sensor_power(should_enable);
        }

        if (!s_sensor_powered) {
            vTaskDelay(delay_ticks(50));
            continue;
        }

        imu_sample_t sample = {};
        esp_err_t err = icm42688p_read_raw(&s_imu, &sample.raw);
        if (err == ESP_OK) {
            sample.timestamp_ms = now_ms();

            float accel_lsb = icm42688p_accel_lsb_per_g(s_imu.accel_fs);
            float gyro_lsb = icm42688p_gyro_lsb_per_dps(s_imu.gyro_fs);
            sample.data.temp_c = icm42688p_raw_temp_to_c(sample.raw.temp);
            sample.data.accel_g[0] = (float)sample.raw.accel_x / accel_lsb;
            sample.data.accel_g[1] = (float)sample.raw.accel_y / accel_lsb;
            sample.data.accel_g[2] = (float)sample.raw.accel_z / accel_lsb;
            sample.data.gyro_dps[0] = (float)sample.raw.gyro_x / gyro_lsb;
            sample.data.gyro_dps[1] = (float)sample.raw.gyro_y / gyro_lsb;
            sample.data.gyro_dps[2] = (float)sample.raw.gyro_z / gyro_lsb;
            update_attitude(&sample);

            message_bus_publish(MSG_TOPIC_IMU, &sample, sizeof(sample));
        } else {
            ESP_LOGW(kTag, "read failed: %s", esp_err_to_name(err));
        }

        vTaskDelay(delay_ticks(s_sample_period_ms));
    }
}
} // namespace

esp_err_t imu_service_start(const imu_service_config_t *cfg)
{
    if (s_started) {
        return ESP_OK;
    }
    if (cfg == nullptr || cfg->sda_pin < 0 || cfg->scl_pin < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t i2c_clk_hz = (cfg->i2c_clk_hz == 0) ? kDefaultI2cHz : cfg->i2c_clk_hz;
    s_sample_period_ms = (cfg->sample_period_ms == 0) ? kDefaultSampleMs : cfg->sample_period_ms;

    esp_err_t err = hal_i2c_master_bus_init(&s_bus,
                                            kI2cPort,
                                            (gpio_num_t)cfg->sda_pin,
                                            (gpio_num_t)cfg->scl_pin,
                                            i2c_clk_hz);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "i2c bus init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = hal_i2c_master_device_add(&s_bus, cfg->i2c_addr, i2c_clk_hz, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "i2c device add failed: %s", esp_err_to_name(err));
        hal_i2c_master_bus_deinit(&s_bus);
        return err;
    }

    icm42688p_config_t imu_cfg = {};
    icm42688p_config_default(&imu_cfg, &s_dev);
    err = icm42688p_begin(&s_imu, &imu_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "icm42688p begin failed: %s", esp_err_to_name(err));
        hal_i2c_master_device_remove(&s_dev);
        hal_i2c_master_bus_deinit(&s_bus);
        return err;
    }

    // Enable on-chip 3rd-order low-pass at ODR/10 (~20 Hz BW with ODR=200 Hz).
    // Hand motion is below ~10 Hz so signal is preserved; everything above is
    // attenuated, killing the high-frequency accel noise that drives Madgwick
    // jitter at rest. Order 3 gives a steeper rolloff than the order-2 default
    // for the same cutoff.
    err = icm42688p_set_ui_filter(&s_imu,
                                  ICM42688P_UI_FILTER_BW_MAX_400_ODR_DIV_10,
                                  ICM42688P_UI_FILTER_BW_MAX_400_ODR_DIV_10,
                                  ICM42688P_UI_FILTER_ORDER_3,
                                  ICM42688P_UI_FILTER_ORDER_3);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "ui filter cfg failed: %s", esp_err_to_name(err));
    }

    s_sensor_powered = true;
    set_sensor_power(s_enabled);

    s_started = true;
    xTaskCreate(imu_task, "imu_service", 4096, nullptr, 5, &s_task);
    ESP_LOGI(kTag, "started addr=0x%02x sda=%d scl=%d hz=%u period=%u",
             cfg->i2c_addr,
             cfg->sda_pin,
             cfg->scl_pin,
             (unsigned)i2c_clk_hz,
             (unsigned)s_sample_period_ms);
    return ESP_OK;
}

void imu_service_set_enabled(bool enabled)
{
    s_enabled = enabled;
}
