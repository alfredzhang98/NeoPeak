#include "icm42688p.h"

#include <string.h>

#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
constexpr uint8_t kBank0 = 0;
constexpr uint8_t kBank1 = 1;

constexpr uint8_t kRegDeviceConfig = 0x11;
constexpr uint8_t kRegDriveConfig = 0x13;
constexpr uint8_t kRegIntConfig = 0x14;
constexpr uint8_t kRegTempData1 = 0x1D;
constexpr uint8_t kRegIntStatus = 0x2D;
constexpr uint8_t kRegApexData4 = 0x35;
constexpr uint8_t kRegIntStatus3 = 0x38;
constexpr uint8_t kRegSignalPathReset = 0x4B;
constexpr uint8_t kRegIntfConfig0 = 0x4C;
constexpr uint8_t kRegIntfConfig1 = 0x4D;
constexpr uint8_t kRegPwrMgmt0 = 0x4E;
constexpr uint8_t kRegGyroConfig0 = 0x4F;
constexpr uint8_t kRegAccelConfig0 = 0x50;
constexpr uint8_t kRegGyroConfig1 = 0x51;
constexpr uint8_t kRegGyroAccelConfig0 = 0x52;
constexpr uint8_t kRegAccelConfig1 = 0x53;
constexpr uint8_t kRegTmstConfig = 0x54;
constexpr uint8_t kRegApexConfig0 = 0x56;
constexpr uint8_t kRegIntConfig1 = 0x64;
constexpr uint8_t kRegIntSource0 = 0x65;
constexpr uint8_t kRegIntSource3 = 0x68;
constexpr uint8_t kRegWhoAmI = 0x75;
constexpr uint8_t kRegBankSel = 0x76;

constexpr uint8_t kRegIntfConfig4 = 0x7A;
constexpr uint8_t kRegIntfConfig5 = 0x7B;
constexpr uint8_t kRegIntfConfig6 = 0x7C;

constexpr uint8_t kRegApexConfig7 = 0x46;
constexpr uint8_t kRegApexConfig8 = 0x47;
constexpr uint8_t kRegIntSource6 = 0x4D;

constexpr uint8_t kBitSoftReset = 0x01;
constexpr uint8_t kBitDataReady = 0x08;
constexpr uint8_t kBitTapDetected = 0x01;
constexpr uint8_t kBitFifoFlush = 0x02;
constexpr uint8_t kBitDmpInit = 0x40;
constexpr uint8_t kBitDmpMemReset = 0x20;
constexpr uint8_t kBitIntAsyncReset = 0x10;
constexpr uint8_t kBitTmstFsyncEn = 0x02;
constexpr uint8_t kBitTapEnable = 0x40;
constexpr uint8_t kBitDmpPowerSave = 0x80;

constexpr uint32_t kDefaultTimeoutMs = 100;
constexpr uint8_t kUnknownBank = 0xFF;

TickType_t delay_ticks(uint32_t ms)
{
    TickType_t ticks = pdMS_TO_TICKS(ms);
    return (ticks == 0) ? 1 : ticks;
}

esp_err_t validate_ctx(icm42688p_t *imu)
{
    if (imu == nullptr || imu->dev == nullptr || imu->dev->handle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

int16_t be16(const uint8_t *data)
{
    return (int16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);
}

bool gyro_fs_valid(icm42688p_gyro_fs_t fs)
{
    return (int)fs >= (int)ICM42688P_GYRO_FS_2000DPS &&
           (int)fs <= (int)ICM42688P_GYRO_FS_15_625DPS;
}

bool accel_fs_valid(icm42688p_accel_fs_t fs)
{
    return (int)fs >= (int)ICM42688P_ACCEL_FS_16G &&
           (int)fs <= (int)ICM42688P_ACCEL_FS_2G;
}

bool odr_valid(icm42688p_odr_t odr)
{
    return (int)odr >= (int)ICM42688P_ODR_32KHZ &&
           (int)odr <= (int)ICM42688P_ODR_500HZ;
}

bool gyro_odr_valid(icm42688p_odr_t odr)
{
    return odr_valid(odr) &&
           odr != ICM42688P_ODR_6_25HZ &&
           odr != ICM42688P_ODR_3_125HZ &&
           odr != ICM42688P_ODR_1_5625HZ;
}

bool accel_odr_valid_for_mode(icm42688p_odr_t odr, icm42688p_accel_mode_t mode)
{
    if (!odr_valid(odr)) {
        return false;
    }
    if (mode == ICM42688P_ACCEL_MODE_LOW_NOISE) {
        return odr != ICM42688P_ODR_6_25HZ &&
               odr != ICM42688P_ODR_3_125HZ &&
               odr != ICM42688P_ODR_1_5625HZ;
    }
    if (mode == ICM42688P_ACCEL_MODE_LOW_POWER) {
        return odr == ICM42688P_ODR_200HZ ||
               odr == ICM42688P_ODR_100HZ ||
               odr == ICM42688P_ODR_50HZ ||
               odr == ICM42688P_ODR_25HZ ||
               odr == ICM42688P_ODR_12_5HZ ||
               odr == ICM42688P_ODR_6_25HZ ||
               odr == ICM42688P_ODR_3_125HZ ||
               odr == ICM42688P_ODR_1_5625HZ ||
               odr == ICM42688P_ODR_500HZ;
    }
    return true;
}

bool gyro_mode_valid(icm42688p_gyro_mode_t mode)
{
    return mode == ICM42688P_GYRO_MODE_OFF ||
           mode == ICM42688P_GYRO_MODE_STANDBY ||
           mode == ICM42688P_GYRO_MODE_LOW_NOISE;
}

bool accel_mode_valid(icm42688p_accel_mode_t mode)
{
    return mode == ICM42688P_ACCEL_MODE_OFF ||
           mode == ICM42688P_ACCEL_MODE_LOW_POWER ||
           mode == ICM42688P_ACCEL_MODE_LOW_NOISE;
}

bool ui_filter_order_valid(icm42688p_ui_filter_order_t order)
{
    return order == ICM42688P_UI_FILTER_ORDER_1 ||
           order == ICM42688P_UI_FILTER_ORDER_2 ||
           order == ICM42688P_UI_FILTER_ORDER_3;
}

bool ui_filter_bw_valid(icm42688p_ui_filter_bw_t bw)
{
    return ((int)bw >= 0 && (int)bw <= 7) ||
           bw == ICM42688P_UI_FILTER_BW_LOW_LATENCY_DEC2 ||
           bw == ICM42688P_UI_FILTER_BW_LOW_LATENCY_8X;
}

bool int_config_valid(icm42688p_int_mode_t mode,
                      icm42688p_int_drive_t drive,
                      icm42688p_int_polarity_t polarity)
{
    return (mode == ICM42688P_INT_MODE_PULSED || mode == ICM42688P_INT_MODE_LATCHED) &&
           (drive == ICM42688P_INT_DRIVE_OPEN_DRAIN || drive == ICM42688P_INT_DRIVE_PUSH_PULL) &&
           (polarity == ICM42688P_INT_POLARITY_ACTIVE_LOW || polarity == ICM42688P_INT_POLARITY_ACTIVE_HIGH);
}

bool int_pin_valid(icm42688p_int_pin_t pin)
{
    return pin == ICM42688P_INT_PIN_1 || pin == ICM42688P_INT_PIN_2;
}

bool int_source_to_reg_mask(icm42688p_int_pin_t pin,
                            icm42688p_int_source_t source,
                            uint8_t *bank,
                            uint8_t *reg,
                            uint8_t *mask)
{
    if (bank == nullptr || reg == nullptr || mask == nullptr || !int_pin_valid(pin)) {
        return false;
    }

    switch (source) {
    case ICM42688P_INT_SOURCE_UI_AGC_RDY: *bank = kBank0; *reg = (pin == ICM42688P_INT_PIN_1) ? kRegIntSource0 : kRegIntSource3; *mask = 0x01; return true;
    case ICM42688P_INT_SOURCE_FIFO_FULL: *bank = kBank0; *reg = (pin == ICM42688P_INT_PIN_1) ? kRegIntSource0 : kRegIntSource3; *mask = 0x02; return true;
    case ICM42688P_INT_SOURCE_FIFO_THS: *bank = kBank0; *reg = (pin == ICM42688P_INT_PIN_1) ? kRegIntSource0 : kRegIntSource3; *mask = 0x04; return true;
    case ICM42688P_INT_SOURCE_UI_DRDY: *bank = kBank0; *reg = (pin == ICM42688P_INT_PIN_1) ? kRegIntSource0 : kRegIntSource3; *mask = 0x08; return true;
    case ICM42688P_INT_SOURCE_RESET_DONE: *bank = kBank0; *reg = (pin == ICM42688P_INT_PIN_1) ? kRegIntSource0 : kRegIntSource3; *mask = 0x10; return true;
    case ICM42688P_INT_SOURCE_PLL_RDY: *bank = kBank0; *reg = (pin == ICM42688P_INT_PIN_1) ? kRegIntSource0 : kRegIntSource3; *mask = 0x20; return true;
    case ICM42688P_INT_SOURCE_UI_FSYNC: *bank = kBank0; *reg = (pin == ICM42688P_INT_PIN_1) ? kRegIntSource0 : kRegIntSource3; *mask = 0x40; return true;
    case ICM42688P_INT_SOURCE_TAP: *bank = 4; *reg = (pin == ICM42688P_INT_PIN_1) ? kRegIntSource6 : 0x4E; *mask = 0x01; return true;
    case ICM42688P_INT_SOURCE_SLEEP: *bank = 4; *reg = (pin == ICM42688P_INT_PIN_1) ? kRegIntSource6 : 0x4E; *mask = 0x02; return true;
    case ICM42688P_INT_SOURCE_WAKE: *bank = 4; *reg = (pin == ICM42688P_INT_PIN_1) ? kRegIntSource6 : 0x4E; *mask = 0x04; return true;
    case ICM42688P_INT_SOURCE_TILT: *bank = 4; *reg = (pin == ICM42688P_INT_PIN_1) ? kRegIntSource6 : 0x4E; *mask = 0x08; return true;
    case ICM42688P_INT_SOURCE_STEP_COUNT_OVERFLOW: *bank = 4; *reg = (pin == ICM42688P_INT_PIN_1) ? kRegIntSource6 : 0x4E; *mask = 0x10; return true;
    case ICM42688P_INT_SOURCE_STEP_DETECT: *bank = 4; *reg = (pin == ICM42688P_INT_PIN_1) ? kRegIntSource6 : 0x4E; *mask = 0x20; return true;
    default: return false;
    }
}

esp_err_t raw_write(icm42688p_t *imu, uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    return hal_i2c_master_device_write(imu->dev, buf, sizeof(buf), imu->timeout_ms);
}

esp_err_t select_bank(icm42688p_t *imu, uint8_t bank)
{
    esp_err_t err = validate_ctx(imu);
    if (err != ESP_OK) {
        return err;
    }
    if (bank > 4) {
        return ESP_ERR_INVALID_ARG;
    }
    if (imu->current_bank == bank) {
        return ESP_OK;
    }

    err = raw_write(imu, kRegBankSel, bank & 0x07);
    if (err == ESP_OK) {
        imu->current_bank = bank;
    }
    return err;
}

esp_err_t update_reg(icm42688p_t *imu, uint8_t bank, uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t current = 0;
    esp_err_t err = icm42688p_read_reg(imu, bank, reg, &current);
    if (err != ESP_OK) {
        return err;
    }

    current = (uint8_t)((current & ~mask) | (value & mask));
    return icm42688p_write_reg(imu, bank, reg, current);
}

esp_err_t configure_i2c_interface(icm42688p_t *imu)
{
    esp_err_t err = icm42688p_write_reg(imu, kBank0, kRegDriveConfig, 0x09);
    if (err != ESP_OK) {
        return err;
    }

    err = update_reg(imu, kBank0, kRegIntfConfig0, 0x03, 0x02);
    if (err != ESP_OK) {
        return err;
    }

    err = update_reg(imu, kBank1, kRegIntfConfig4, 0x40, 0x00);
    if (err != ESP_OK) {
        return err;
    }

    err = update_reg(imu, kBank1, kRegIntfConfig5, 0x06, 0x00);
    if (err != ESP_OK) {
        return err;
    }

    return update_reg(imu, kBank1, kRegIntfConfig6, 0x03, 0x00);
}
} // namespace

void icm42688p_config_default(icm42688p_config_t *cfg, hal_i2c_device_t *dev)
{
    if (cfg == nullptr) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->dev = dev;
    cfg->timeout_ms = kDefaultTimeoutMs;
    cfg->reset_on_begin = true;
    cfg->configure_i2c_interface = true;
    cfg->enable_temp = true;
    cfg->gyro_fs = ICM42688P_GYRO_FS_2000DPS;
    cfg->accel_fs = ICM42688P_ACCEL_FS_16G;
    cfg->gyro_odr = ICM42688P_ODR_200HZ;
    cfg->accel_odr = ICM42688P_ODR_200HZ;
    cfg->gyro_mode = ICM42688P_GYRO_MODE_LOW_NOISE;
    cfg->accel_mode = ICM42688P_ACCEL_MODE_LOW_NOISE;
}

esp_err_t icm42688p_begin(icm42688p_t *imu, const icm42688p_config_t *cfg)
{
    if (imu == nullptr || cfg == nullptr || cfg->dev == nullptr || cfg->dev->handle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!gyro_fs_valid(cfg->gyro_fs) || !accel_fs_valid(cfg->accel_fs) ||
        !gyro_odr_valid(cfg->gyro_odr) ||
        !accel_odr_valid_for_mode(cfg->accel_odr, cfg->accel_mode) ||
        !gyro_mode_valid(cfg->gyro_mode) || !accel_mode_valid(cfg->accel_mode)) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(imu, 0, sizeof(*imu));
    imu->dev = cfg->dev;
    imu->timeout_ms = (cfg->timeout_ms == 0) ? kDefaultTimeoutMs : cfg->timeout_ms;
    imu->current_bank = kUnknownBank;
    imu->gyro_fs = cfg->gyro_fs;
    imu->accel_fs = cfg->accel_fs;
    imu->gyro_mode = ICM42688P_GYRO_MODE_OFF;
    imu->accel_mode = ICM42688P_ACCEL_MODE_OFF;
    imu->temp_enabled = true;

    esp_err_t err = ESP_OK;
    if (cfg->reset_on_begin) {
        err = icm42688p_soft_reset(imu);
        if (err != ESP_OK) {
            return err;
        }
    }

    err = icm42688p_check_who_am_i(imu);
    if (err != ESP_OK) {
        return err;
    }

    err = icm42688p_set_power(imu, ICM42688P_GYRO_MODE_OFF, ICM42688P_ACCEL_MODE_OFF, cfg->enable_temp);
    if (err != ESP_OK) {
        return err;
    }

    if (cfg->configure_i2c_interface) {
        err = configure_i2c_interface(imu);
        if (err != ESP_OK) {
            return err;
        }
    }

    err = update_reg(imu, kBank0, kRegIntConfig1, kBitIntAsyncReset, 0x00);
    if (err != ESP_OK) {
        return err;
    }

    err = update_reg(imu, kBank0, kRegTmstConfig, kBitTmstFsyncEn, 0x00);
    if (err != ESP_OK) {
        return err;
    }

    err = icm42688p_set_sensor_config(imu, cfg->gyro_fs, cfg->accel_fs, cfg->gyro_odr, cfg->accel_odr);
    if (err != ESP_OK) {
        return err;
    }

    if (cfg->accel_mode == ICM42688P_ACCEL_MODE_LOW_POWER) {
        err = update_reg(imu, kBank0, kRegGyroAccelConfig0, 0xF0, 0x60);
        if (err != ESP_OK) {
            return err;
        }
    }

    err = icm42688p_set_power(imu, cfg->gyro_mode, cfg->accel_mode, cfg->enable_temp);
    if (err != ESP_OK) {
        return err;
    }

    imu->initialized = true;
    return ESP_OK;
}

void icm42688p_deinit(icm42688p_t *imu)
{
    if (imu == nullptr) {
        return;
    }
    memset(imu, 0, sizeof(*imu));
    imu->current_bank = kUnknownBank;
}

esp_err_t icm42688p_read_reg(icm42688p_t *imu, uint8_t bank, uint8_t reg, uint8_t *value)
{
    if (value == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    return icm42688p_read_regs(imu, bank, reg, value, 1);
}

esp_err_t icm42688p_write_reg(icm42688p_t *imu, uint8_t bank, uint8_t reg, uint8_t value)
{
    esp_err_t err = select_bank(imu, bank);
    if (err != ESP_OK) {
        return err;
    }

    return raw_write(imu, reg, value);
}

esp_err_t icm42688p_read_regs(icm42688p_t *imu, uint8_t bank, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (data == nullptr || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = select_bank(imu, bank);
    if (err != ESP_OK) {
        return err;
    }

    return hal_i2c_master_device_write_read(imu->dev, &reg, 1, data, len, imu->timeout_ms);
}

esp_err_t icm42688p_soft_reset(icm42688p_t *imu)
{
    esp_err_t err = icm42688p_write_reg(imu, kBank0, kRegDeviceConfig, kBitSoftReset);
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(delay_ticks(2));
    imu->current_bank = kUnknownBank;
    return ESP_OK;
}

esp_err_t icm42688p_read_who_am_i(icm42688p_t *imu, uint8_t *who_am_i)
{
    return icm42688p_read_reg(imu, kBank0, kRegWhoAmI, who_am_i);
}

esp_err_t icm42688p_check_who_am_i(icm42688p_t *imu)
{
    uint8_t who_am_i = 0;
    esp_err_t err = icm42688p_read_who_am_i(imu, &who_am_i);
    if (err != ESP_OK) {
        return err;
    }

    return (who_am_i == ICM42688P_WHO_AM_I_VALUE) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t icm42688p_set_sensor_config(icm42688p_t *imu,
                                      icm42688p_gyro_fs_t gyro_fs,
                                      icm42688p_accel_fs_t accel_fs,
                                      icm42688p_odr_t gyro_odr,
                                      icm42688p_odr_t accel_odr)
{
    esp_err_t err = validate_ctx(imu);
    if (err != ESP_OK) {
        return err;
    }
    if (!gyro_fs_valid(gyro_fs) || !accel_fs_valid(accel_fs) ||
        !gyro_odr_valid(gyro_odr) || !accel_odr_valid_for_mode(accel_odr, imu->accel_mode)) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t gyro_cfg = (uint8_t)(((uint8_t)gyro_fs << 5) | ((uint8_t)gyro_odr & 0x0F));
    uint8_t accel_cfg = (uint8_t)(((uint8_t)accel_fs << 5) | ((uint8_t)accel_odr & 0x0F));

    err = icm42688p_write_reg(imu, kBank0, kRegGyroConfig0, gyro_cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = icm42688p_write_reg(imu, kBank0, kRegAccelConfig0, accel_cfg);
    if (err != ESP_OK) {
        return err;
    }

    imu->gyro_fs = gyro_fs;
    imu->accel_fs = accel_fs;
    return ESP_OK;
}

esp_err_t icm42688p_set_power(icm42688p_t *imu,
                              icm42688p_gyro_mode_t gyro_mode,
                              icm42688p_accel_mode_t accel_mode,
                              bool temp_enable)
{
    esp_err_t err = validate_ctx(imu);
    if (err != ESP_OK) {
        return err;
    }
    if (!gyro_mode_valid(gyro_mode) || !accel_mode_valid(accel_mode)) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t value = 0;
    if (!temp_enable) {
        value |= 0x20;
    }
    value |= (uint8_t)(((uint8_t)gyro_mode & 0x03) << 2);
    value |= (uint8_t)((uint8_t)accel_mode & 0x03);

    err = icm42688p_write_reg(imu, kBank0, kRegPwrMgmt0, value);
    if (err != ESP_OK) {
        return err;
    }

    esp_rom_delay_us(200);
    if (gyro_mode != ICM42688P_GYRO_MODE_OFF && imu->gyro_mode == ICM42688P_GYRO_MODE_OFF) {
        vTaskDelay(delay_ticks(45));
    }

    imu->gyro_mode = gyro_mode;
    imu->accel_mode = accel_mode;
    imu->temp_enabled = temp_enable;
    return ESP_OK;
}

esp_err_t icm42688p_set_ui_filter(icm42688p_t *imu,
                                  icm42688p_ui_filter_bw_t gyro_bw,
                                  icm42688p_ui_filter_bw_t accel_bw,
                                  icm42688p_ui_filter_order_t gyro_order,
                                  icm42688p_ui_filter_order_t accel_order)
{
    esp_err_t err = validate_ctx(imu);
    if (err != ESP_OK) {
        return err;
    }
    if (!ui_filter_bw_valid(gyro_bw) || !ui_filter_bw_valid(accel_bw) ||
        !ui_filter_order_valid(gyro_order) || !ui_filter_order_valid(accel_order)) {
        return ESP_ERR_INVALID_ARG;
    }

    err = icm42688p_write_reg(imu,
                              kBank0,
                              kRegGyroAccelConfig0,
                              (uint8_t)((((uint8_t)accel_bw & 0x0F) << 4) | ((uint8_t)gyro_bw & 0x0F)));
    if (err != ESP_OK) {
        return err;
    }

    err = update_reg(imu, kBank0, kRegGyroConfig1, 0x0C, (uint8_t)(((uint8_t)gyro_order & 0x03) << 2));
    if (err != ESP_OK) {
        return err;
    }

    return update_reg(imu, kBank0, kRegAccelConfig1, 0x18, (uint8_t)(((uint8_t)accel_order & 0x03) << 3));
}

esp_err_t icm42688p_config_int(icm42688p_t *imu,
                               icm42688p_int_pin_t pin,
                               icm42688p_int_mode_t mode,
                               icm42688p_int_drive_t drive,
                               icm42688p_int_polarity_t polarity)
{
    esp_err_t err = validate_ctx(imu);
    if (err != ESP_OK) {
        return err;
    }
    if (!int_pin_valid(pin) || !int_config_valid(mode, drive, polarity)) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t shift = (pin == ICM42688P_INT_PIN_1) ? 0 : 3;
    uint8_t mask = (uint8_t)(0x07 << shift);
    uint8_t value = (uint8_t)((((uint8_t)mode << 2) | ((uint8_t)drive << 1) | (uint8_t)polarity) << shift);
    return update_reg(imu, kBank0, kRegIntConfig, mask, value);
}

esp_err_t icm42688p_config_int1(icm42688p_t *imu,
                                icm42688p_int_mode_t mode,
                                icm42688p_int_drive_t drive,
                                icm42688p_int_polarity_t polarity)
{
    return icm42688p_config_int(imu, ICM42688P_INT_PIN_1, mode, drive, polarity);
}

esp_err_t icm42688p_route_interrupt(icm42688p_t *imu,
                                    icm42688p_int_pin_t pin,
                                    icm42688p_int_source_t source,
                                    bool enable)
{
    esp_err_t err = validate_ctx(imu);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t bank = 0;
    uint8_t reg = 0;
    uint8_t mask = 0;
    if (!int_source_to_reg_mask(pin, source, &bank, &reg, &mask)) {
        return ESP_ERR_INVALID_ARG;
    }

    return update_reg(imu, bank, reg, mask, enable ? mask : 0x00);
}

esp_err_t icm42688p_apex_init(icm42688p_t *imu)
{
    esp_err_t err = validate_ctx(imu);
    if (err != ESP_OK) {
        return err;
    }

    err = update_reg(imu, kBank0, kRegApexConfig0, kBitDmpPowerSave, 0x00);
    if (err != ESP_OK) {
        return err;
    }

    err = icm42688p_write_reg(imu, kBank0, kRegSignalPathReset, kBitDmpMemReset);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(delay_ticks(1));

    err = icm42688p_write_reg(imu, kBank0, kRegSignalPathReset, kBitDmpInit);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(delay_ticks(1));
    return ESP_OK;
}

esp_err_t icm42688p_tap_config_default(icm42688p_tap_config_t *cfg)
{
    if (cfg == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    cfg->min_jerk_thr = 17;
    cfg->max_peak_tol = 2;
    cfg->tmax = 2;
    cfg->tavg = 3;
    cfg->tmin = 3;
    return ESP_OK;
}

esp_err_t icm42688p_set_tap_config(icm42688p_t *imu, const icm42688p_tap_config_t *cfg)
{
    esp_err_t err = validate_ctx(imu);
    if (err != ESP_OK) {
        return err;
    }

    icm42688p_tap_config_t local_cfg = {};
    if (cfg == nullptr) {
        err = icm42688p_tap_config_default(&local_cfg);
        if (err != ESP_OK) {
            return err;
        }
        cfg = &local_cfg;
    }
    if (cfg->min_jerk_thr > 0x3F || cfg->max_peak_tol > 0x03 ||
        cfg->tmax > 0x03 || cfg->tavg > 0x03 || cfg->tmin > 0x07) {
        return ESP_ERR_INVALID_ARG;
    }

    err = icm42688p_write_reg(imu,
                              4,
                              kRegApexConfig7,
                              (uint8_t)(((cfg->min_jerk_thr & 0x3F) << 2) | (cfg->max_peak_tol & 0x03)));
    if (err != ESP_OK) {
        return err;
    }

    return icm42688p_write_reg(imu,
                               4,
                               kRegApexConfig8,
                               (uint8_t)(((cfg->tmax & 0x03) << 5) |
                                         ((cfg->tavg & 0x03) << 3) |
                                         (cfg->tmin & 0x07)));
}

esp_err_t icm42688p_set_tap_enable(icm42688p_t *imu, bool enable)
{
    return update_reg(imu, kBank0, kRegApexConfig0, kBitTapEnable, enable ? kBitTapEnable : 0x00);
}

esp_err_t icm42688p_enable_tap_int1(icm42688p_t *imu, const icm42688p_tap_config_t *cfg)
{
    esp_err_t err = icm42688p_config_int(imu,
                                         ICM42688P_INT_PIN_1,
                                         ICM42688P_INT_MODE_LATCHED,
                                         ICM42688P_INT_DRIVE_PUSH_PULL,
                                         ICM42688P_INT_POLARITY_ACTIVE_HIGH);
    if (err != ESP_OK) {
        return err;
    }

    err = icm42688p_set_sensor_config(imu,
                                      imu->gyro_fs,
                                      imu->accel_fs,
                                      ICM42688P_ODR_1KHZ,
                                      ICM42688P_ODR_500HZ);
    if (err != ESP_OK) {
        return err;
    }

    err = update_reg(imu, kBank0, kRegIntfConfig1, 0x08, 0x00);
    if (err != ESP_OK) {
        return err;
    }

    err = icm42688p_set_power(imu, imu->gyro_mode, ICM42688P_ACCEL_MODE_LOW_POWER, imu->temp_enabled);
    if (err != ESP_OK) {
        return err;
    }

    err = update_reg(imu, kBank0, kRegAccelConfig1, 0x06, 0x04);
    if (err != ESP_OK) {
        return err;
    }

    err = update_reg(imu, kBank0, kRegGyroAccelConfig0, 0xF0, 0x40);
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(delay_ticks(1));

    err = icm42688p_apex_init(imu);
    if (err != ESP_OK) {
        return err;
    }

    err = icm42688p_set_tap_config(imu, cfg);
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(delay_ticks(1));

    err = icm42688p_route_interrupt(imu, ICM42688P_INT_PIN_1, ICM42688P_INT_SOURCE_TAP, true);
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(delay_ticks(50));
    return icm42688p_set_tap_enable(imu, true);
}

esp_err_t icm42688p_disable_tap(icm42688p_t *imu)
{
    esp_err_t err = icm42688p_set_tap_enable(imu, false);
    if (err != ESP_OK) {
        return err;
    }

    return icm42688p_route_interrupt(imu, ICM42688P_INT_PIN_1, ICM42688P_INT_SOURCE_TAP, false);
}

esp_err_t icm42688p_read_tap_event(icm42688p_t *imu, icm42688p_tap_event_t *event)
{
    if (event == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t status3 = 0;
    esp_err_t err = icm42688p_read_int_status3(imu, &status3);
    if (err != ESP_OK) {
        return err;
    }
    if ((status3 & kBitTapDetected) == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t raw = 0;
    err = icm42688p_read_reg(imu, kBank0, kRegApexData4, &raw);
    if (err != ESP_OK) {
        return err;
    }

    event->raw = raw;
    event->num = (icm42688p_tap_num_t)((raw >> 3) & 0x03);
    event->axis = (icm42688p_tap_axis_t)((raw >> 1) & 0x03);
    event->negative = (raw & 0x01) != 0;
    return ESP_OK;
}

esp_err_t icm42688p_read_int_status(icm42688p_t *imu, uint8_t *status)
{
    return icm42688p_read_reg(imu, kBank0, kRegIntStatus, status);
}

esp_err_t icm42688p_read_int_status3(icm42688p_t *imu, uint8_t *status3)
{
    return icm42688p_read_reg(imu, kBank0, kRegIntStatus3, status3);
}

esp_err_t icm42688p_data_ready(icm42688p_t *imu, bool *ready)
{
    if (ready == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t status = 0;
    esp_err_t err = icm42688p_read_int_status(imu, &status);
    if (err != ESP_OK) {
        return err;
    }

    *ready = (status & kBitDataReady) != 0;
    return ESP_OK;
}

esp_err_t icm42688p_flush_fifo(icm42688p_t *imu)
{
    return icm42688p_write_reg(imu, kBank0, kRegSignalPathReset, kBitFifoFlush);
}

esp_err_t icm42688p_read_raw(icm42688p_t *imu, icm42688p_raw_data_t *raw)
{
    if (raw == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buf[14] = {};
    esp_err_t err = icm42688p_read_regs(imu, kBank0, kRegTempData1, buf, sizeof(buf));
    if (err != ESP_OK) {
        return err;
    }

    raw->temp = be16(&buf[0]);
    raw->accel_x = be16(&buf[2]);
    raw->accel_y = be16(&buf[4]);
    raw->accel_z = be16(&buf[6]);
    raw->gyro_x = be16(&buf[8]);
    raw->gyro_y = be16(&buf[10]);
    raw->gyro_z = be16(&buf[12]);
    return ESP_OK;
}

esp_err_t icm42688p_read(icm42688p_t *imu, icm42688p_data_t *data)
{
    if (data == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    icm42688p_raw_data_t raw = {};
    esp_err_t err = icm42688p_read_raw(imu, &raw);
    if (err != ESP_OK) {
        return err;
    }

    float accel_lsb = icm42688p_accel_lsb_per_g(imu->accel_fs);
    float gyro_lsb = icm42688p_gyro_lsb_per_dps(imu->gyro_fs);

    data->temp_c = icm42688p_raw_temp_to_c(raw.temp);
    data->accel_g[0] = (float)raw.accel_x / accel_lsb;
    data->accel_g[1] = (float)raw.accel_y / accel_lsb;
    data->accel_g[2] = (float)raw.accel_z / accel_lsb;
    data->gyro_dps[0] = (float)raw.gyro_x / gyro_lsb;
    data->gyro_dps[1] = (float)raw.gyro_y / gyro_lsb;
    data->gyro_dps[2] = (float)raw.gyro_z / gyro_lsb;
    return ESP_OK;
}

float icm42688p_raw_temp_to_c(int16_t raw_temp)
{
    return ((float)raw_temp / 132.48f) + 25.0f;
}

float icm42688p_accel_lsb_per_g(icm42688p_accel_fs_t fs)
{
    switch (fs) {
    case ICM42688P_ACCEL_FS_16G: return 2048.0f;
    case ICM42688P_ACCEL_FS_8G: return 4096.0f;
    case ICM42688P_ACCEL_FS_4G: return 8192.0f;
    case ICM42688P_ACCEL_FS_2G: return 16384.0f;
    default: return 2048.0f;
    }
}

float icm42688p_gyro_lsb_per_dps(icm42688p_gyro_fs_t fs)
{
    switch (fs) {
    case ICM42688P_GYRO_FS_2000DPS: return 16.4f;
    case ICM42688P_GYRO_FS_1000DPS: return 32.8f;
    case ICM42688P_GYRO_FS_500DPS: return 65.5f;
    case ICM42688P_GYRO_FS_250DPS: return 131.0f;
    case ICM42688P_GYRO_FS_125DPS: return 262.0f;
    case ICM42688P_GYRO_FS_62_5DPS: return 524.3f;
    case ICM42688P_GYRO_FS_31_25DPS: return 1048.6f;
    case ICM42688P_GYRO_FS_15_625DPS: return 2097.2f;
    default: return 16.4f;
    }
}
