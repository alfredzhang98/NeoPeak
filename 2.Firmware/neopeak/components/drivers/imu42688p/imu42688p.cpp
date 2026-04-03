#include "imu42688p.hpp"

#include "esp_log.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
using namespace imu42688p_reg;
constexpr uint8_t kImuId = 0x47;
constexpr uint8_t kBank0 = 0;
constexpr uint8_t kBank4 = 4;
constexpr uint32_t kDefaultI2cHz = 400000;
constexpr uint32_t kI2cTimeoutMs = 20;
constexpr const char *kTag = "imu42688p";

inline int16_t read_be_s16(const uint8_t *data)
{
    return (int16_t)((uint16_t)data[0] << 8) | (int16_t)data[1];
}
} // namespace

Imu42688p::Imu42688p()
    : device_({}),
      initialized_(false),
      gyro_range_(4000.0f / 65535.0f),
      accel_range_(0.488f),
      int_pin_(1),
      fifo_mode_(false)
{
}

esp_err_t Imu42688p::begin(hal_i2c_bus_t *bus, uint8_t addr, uint32_t i2c_hz)
{
    if (bus == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (i2c_hz == 0) {
        i2c_hz = kDefaultI2cHz;
    }

    esp_err_t err = hal_i2c_master_device_add(bus, addr, i2c_hz, &device_);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "i2c add device failed: %s", esp_err_to_name(err));
        return err;
    }

    initialized_ = true;

    auto cleanup_on_fail = [this]() {
        hal_i2c_master_device_remove(&device_);
        initialized_ = false;
    };

    uint8_t bank = kBank0;
    err = write_reg_u8(kRegBankSel, bank);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "reg bank select failed: %s", esp_err_to_name(err));
        cleanup_on_fail();
        return err;
    }

    uint8_t id = 0;
    err = read_reg(kWhoAmI, &id, 1);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "whoami read failed: %s", esp_err_to_name(err));
        cleanup_on_fail();
        return err;
    }

    if (id != kImuId) {
        ESP_LOGE(kTag, "whoami mismatch: 0x%02X", id);
        cleanup_on_fail();
        return ESP_ERR_INVALID_RESPONSE;
    }

    err = write_reg_u8(kDeviceConfig, 0);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "device config write failed: %s", esp_err_to_name(err));
        cleanup_on_fail();
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(2));
    return ESP_OK;
}

esp_err_t Imu42688p::read_temperature(float *out_c)
{
    if (!initialized_ || out_c == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    if (fifo_mode_) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data[2] = {0};
    esp_err_t err = read_reg(kTempData1, data, 2);
    if (err != ESP_OK) {
        return err;
    }

    int16_t raw = read_be_s16(data);
    *out_c = (float)raw / 132.48f + 25.0f;
    return ESP_OK;
}

esp_err_t Imu42688p::read_accel(Vec3f *out_g)
{
    if (!initialized_ || out_g == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    if (fifo_mode_) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data[6] = {0};
    esp_err_t err = read_reg(kAccelDataX1, data, 6);
    if (err != ESP_OK) {
        return err;
    }

    out_g->x = (float)read_be_s16(&data[0]) * accel_range_;
    out_g->y = (float)read_be_s16(&data[2]) * accel_range_;
    out_g->z = (float)read_be_s16(&data[4]) * accel_range_;
    return ESP_OK;
}

esp_err_t Imu42688p::read_gyro(Vec3f *out_dps)
{
    if (!initialized_ || out_dps == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    if (fifo_mode_) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data[6] = {0};
    esp_err_t err = read_reg(kGyroDataX1, data, 6);
    if (err != ESP_OK) {
        return err;
    }

    out_dps->x = (float)read_be_s16(&data[0]) * gyro_range_;
    out_dps->y = (float)read_be_s16(&data[2]) * gyro_range_;
    out_dps->z = (float)read_be_s16(&data[4]) * gyro_range_;
    return ESP_OK;
}

esp_err_t Imu42688p::read_all(Vec3f *out_g, Vec3f *out_dps, float *out_c)
{
    if (!initialized_ || out_g == nullptr || out_dps == nullptr || out_c == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    if (fifo_mode_) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data[14] = {0};
    esp_err_t err = read_reg(kTempData1, data, 14);
    if (err != ESP_OK) {
        return err;
    }

    int16_t temp_raw = read_be_s16(&data[0]);
    *out_c = (float)temp_raw / 132.48f + 25.0f;

    out_g->x = (float)read_be_s16(&data[2]) * accel_range_;
    out_g->y = (float)read_be_s16(&data[4]) * accel_range_;
    out_g->z = (float)read_be_s16(&data[6]) * accel_range_;

    out_dps->x = (float)read_be_s16(&data[8]) * gyro_range_;
    out_dps->y = (float)read_be_s16(&data[10]) * gyro_range_;
    out_dps->z = (float)read_be_s16(&data[12]) * gyro_range_;
    return ESP_OK;
}

esp_err_t Imu42688p::tap_init(bool low_power_mode)
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t bank = kBank0;
    esp_err_t err = write_reg_u8(kRegBankSel, bank);
    if (err != ESP_OK) {
        return err;
    }

    if (low_power_mode) {
        uint8_t accel_cfg0 = 15;
        err = write_reg_u8(kAccelConfig0, accel_cfg0);
        if (err != ESP_OK) {
            return err;
        }

        uint8_t pwr = 2;
        err = write_reg_u8(kPwrMgmt0, pwr);
        if (err != ESP_OK) {
            return err;
        }

        vTaskDelay(pdMS_TO_TICKS(1));

        uint8_t intf_cfg1 = 0;
        err = write_reg_u8(kIntfConfig1, intf_cfg1);
        if (err != ESP_OK) {
            return err;
        }

        uint8_t accel_cfg1 = 2;
        err = write_reg_u8(kAccelConfig1, accel_cfg1);
        if (err != ESP_OK) {
            return err;
        }

        uint8_t gyro_accel_cfg0 = 0;
        err = write_reg_u8(kGyroAccelConfig0, gyro_accel_cfg0);
        if (err != ESP_OK) {
            return err;
        }
    } else {
        uint8_t accel_cfg0 = 6;
        err = write_reg_u8(kAccelConfig0, accel_cfg0);
        if (err != ESP_OK) {
            return err;
        }

        uint8_t pwr = 3;
        err = write_reg_u8(kPwrMgmt0, pwr);
        if (err != ESP_OK) {
            return err;
        }

        vTaskDelay(pdMS_TO_TICKS(1));

        uint8_t accel_cfg1 = 2;
        err = write_reg_u8(kAccelConfig1, accel_cfg1);
        if (err != ESP_OK) {
            return err;
        }

        uint8_t gyro_accel_cfg0 = 0;
        err = write_reg_u8(kGyroAccelConfig0, gyro_accel_cfg0);
        if (err != ESP_OK) {
            return err;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(1));

    bank = kBank4;
    err = write_reg_u8(kRegBankSel, bank);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t apex8 = (3u) | (3u << 3) | (2u << 5);
    err = write_reg_u8(kApexConfig8, apex8);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t apex7 = (17u << 2) | 1u;
    err = write_reg_u8(kApexConfig7, apex7);
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(1));

    uint8_t tap_en = 1;
    if (int_pin_ == 1) {
        err = write_reg_u8(kIntSource6, tap_en);
    } else {
        err = write_reg_u8(kIntSource7, tap_en);
    }
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    bank = kBank0;
    err = write_reg_u8(kRegBankSel, bank);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t apex0 = 1u << 5;
    return write_reg_u8(kApexConfig0, apex0);
}

esp_err_t Imu42688p::get_tap_info(TapInfo *info)
{
    if (!initialized_ || info == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data = 0;
    esp_err_t err = read_reg(kApexData4, &data, 1);
    if (err != ESP_OK) {
        return err;
    }

    info->num = data & 0x18;
    info->axis = data & 0x06;
    info->direction = data & 0x01;
    return ESP_OK;
}

esp_err_t Imu42688p::wom_init(void)
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t bank = kBank0;
    esp_err_t err = write_reg_u8(kRegBankSel, bank);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t accel_cfg0 = 9;
    err = write_reg_u8(kAccelConfig0, accel_cfg0);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t pwr = 2;
    err = write_reg_u8(kPwrMgmt0, pwr);
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(1));

    uint8_t intf_cfg1 = 0;
    err = write_reg_u8(kIntfConfig1, intf_cfg1);
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(1));
    return ESP_OK;
}

esp_err_t Imu42688p::set_wom_threshold(uint8_t axis, uint8_t threshold)
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t bank = kBank4;
    esp_err_t err = write_reg_u8(kRegBankSel, bank);
    if (err != ESP_OK) {
        return err;
    }

    if (axis == AxisX) {
        err = write_reg_u8(kAccelWomXThr, threshold);
    } else if (axis == AxisY) {
        err = write_reg_u8(kAccelWomYThr, threshold);
    } else if (axis == AxisZ) {
        err = write_reg_u8(kAccelWomZThr, threshold);
    } else if (axis == AxisAll) {
        err = write_reg_u8(kAccelWomXThr, threshold);
        if (err != ESP_OK) {
            return err;
        }
        err = write_reg_u8(kAccelWomYThr, threshold);
        if (err != ESP_OK) {
            return err;
        }
        err = write_reg_u8(kAccelWomZThr, threshold);
    } else {
        err = ESP_ERR_INVALID_ARG;
    }

    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(1));
    return write_reg_u8(kRegBankSel, kBank0);
}

esp_err_t Imu42688p::enable_wom_interrupt(uint8_t axis)
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t bank = kBank0;
    esp_err_t err = write_reg_u8(kRegBankSel, bank);
    if (err != ESP_OK) {
        return err;
    }

    if (int_pin_ == 1) {
        err = write_reg_u8(kIntSource1, axis);
    } else {
        err = write_reg_u8(kIntSource4, axis);
    }
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    uint8_t smd_cfg = 0;
    smd_cfg |= (1u << 2);
    smd_cfg |= 1u;
    return write_reg_u8(kSmdConfig, smd_cfg);
}

esp_err_t Imu42688p::fifo_start(void)
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t bank = kBank0;
    esp_err_t err = write_reg_u8(kRegBankSel, bank);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t fifo_cfg1 = 0;
    fifo_cfg1 |= (1u << 0);
    fifo_cfg1 |= (1u << 1);
    fifo_cfg1 |= (1u << 2);
    err = write_reg_u8(kFifoConfig1, fifo_cfg1);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t fifo_start = 1u << 6;
    err = write_reg_u8(kFifoConfig, fifo_start);
    if (err != ESP_OK) {
        return err;
    }

    fifo_mode_ = true;
    return ESP_OK;
}

esp_err_t Imu42688p::fifo_stop(void)
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t bank = kBank0;
    esp_err_t err = write_reg_u8(kRegBankSel, bank);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t fifo_stop = 1u << 7;
    err = write_reg_u8(kFifoConfig, fifo_stop);
    if (err != ESP_OK) {
        return err;
    }

    fifo_mode_ = false;
    return ESP_OK;
}

esp_err_t Imu42688p::fifo_read(Vec3f *out_g, Vec3f *out_dps, float *out_c)
{
    if (!initialized_ || out_g == nullptr || out_dps == nullptr || out_c == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data[16] = {0};
    esp_err_t err = read_reg(kFifoData, data, 16);
    if (err != ESP_OK) {
        return err;
    }

    out_g->x = (float)read_be_s16(&data[1]) * accel_range_;
    out_g->y = (float)read_be_s16(&data[3]) * accel_range_;
    out_g->z = (float)read_be_s16(&data[5]) * accel_range_;

    out_dps->x = (float)read_be_s16(&data[7]) * gyro_range_;
    out_dps->y = (float)read_be_s16(&data[9]) * gyro_range_;
    out_dps->z = (float)read_be_s16(&data[11]) * gyro_range_;

    *out_c = (float)data[13] / 2.07f + 25.0f;
    return ESP_OK;
}

esp_err_t Imu42688p::write_reg(uint8_t reg, const uint8_t *data, size_t len)
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buffer[1 + 16] = {0};
    if (len > 16 || data == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    buffer[0] = reg;
    for (size_t i = 0; i < len; ++i) {
        buffer[i + 1] = data[i];
    }

    return hal_i2c_master_device_write(&device_, buffer, len + 1, kI2cTimeoutMs);
}

esp_err_t Imu42688p::read_reg(uint8_t reg, uint8_t *data, size_t len)
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    return hal_i2c_master_device_write_read(&device_, &reg, 1, data, len, kI2cTimeoutMs);
}

esp_err_t Imu42688p::write_reg_u8(uint8_t reg, uint8_t value)
{
    return write_reg(reg, &value, 1);
}
