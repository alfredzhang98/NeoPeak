#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "hal_i2c.h"

/**
 * @brief 寄存器地址定义（参考 ICM42688P 数据手册）
 */
namespace imu42688p_reg {
/**
 * @brief Register: DEVICE_CONFIG
 * @n        设备配置（包含软复位等）
 */
constexpr uint8_t kDeviceConfig = 0x11;

/**
 * @brief Register: PWR_MGMT0
 * @n        电源管理配置（陀螺仪/加速度计工作模式）
 */
constexpr uint8_t kPwrMgmt0 = 0x4E;

/**
 * @brief Register: WHO_AM_I
 * @n        设备 ID 读取
 */
constexpr uint8_t kWhoAmI = 0x75;

/**
 * @brief Register: REG_BANK_SEL
 * @n        寄存器 Bank 切换
 */
constexpr uint8_t kRegBankSel = 0x76;

/**
 * @brief Register: TEMP_DATA1
 * @n        温度数据高字节
 */
constexpr uint8_t kTempData1 = 0x1D;

/**
 * @brief Register: ACCEL_DATA_X1
 * @n        X 轴加速度高字节
 */
constexpr uint8_t kAccelDataX1 = 0x1F;

/**
 * @brief Register: ACCEL_DATA_Y1
 * @n        Y 轴加速度高字节
 */
constexpr uint8_t kAccelDataY1 = 0x21;

/**
 * @brief Register: ACCEL_DATA_Z1
 * @n        Z 轴加速度高字节
 */
constexpr uint8_t kAccelDataZ1 = 0x23;

/**
 * @brief Register: GYRO_DATA_X1
 * @n        X 轴陀螺仪高字节
 */
constexpr uint8_t kGyroDataX1 = 0x25;

/**
 * @brief Register: GYRO_DATA_Y1
 * @n        Y 轴陀螺仪高字节
 */
constexpr uint8_t kGyroDataY1 = 0x27;

/**
 * @brief Register: GYRO_DATA_Z1
 * @n        Z 轴陀螺仪高字节
 */
constexpr uint8_t kGyroDataZ1 = 0x29;

/**
 * @brief Register: GYRO_CONFIG0
 * @n        陀螺仪 ODR / 量程配置
 */
constexpr uint8_t kGyroConfig0 = 0x4F;

/**
 * @brief Register: ACCEL_CONFIG0
 * @n        加速度计 ODR / 量程配置
 */
constexpr uint8_t kAccelConfig0 = 0x50;

/**
 * @brief Register: GYRO_ACCEL_CONFIG0
 * @n        陀螺仪/加速度计滤波配置
 */
constexpr uint8_t kGyroAccelConfig0 = 0x52;

/**
 * @brief Register: ACCEL_CONFIG1
 * @n        加速度计滤波配置
 */
constexpr uint8_t kAccelConfig1 = 0x53;

/**
 * @brief Register: INTF_CONFIG1
 * @n        接口相关配置
 */
constexpr uint8_t kIntfConfig1 = 0x4D;

/**
 * @brief Register: SMD_CONFIG
 * @n        重要运动检测与 WOM 配置
 */
constexpr uint8_t kSmdConfig = 0x57;

/**
 * @brief Register: INT_SOURCE1
 * @n        INT1 中断源配置
 */
constexpr uint8_t kIntSource1 = 0x66;

/**
 * @brief Register: INT_SOURCE4
 * @n        INT2 中断源配置
 */
constexpr uint8_t kIntSource4 = 0x69;

/**
 * @brief Register: INT_SOURCE6
 * @n        INT1 敲击检测中断源
 */
constexpr uint8_t kIntSource6 = 0x4D;

/**
 * @brief Register: INT_SOURCE7
 * @n        INT2 敲击检测中断源
 */
constexpr uint8_t kIntSource7 = 0x4E;

/**
 * @brief Register: APEX_CONFIG0
 * @n        APEX 功能开关（Tap/Pedometer/Tilt 等）
 */
constexpr uint8_t kApexConfig0 = 0x56;

/**
 * @brief Register: APEX_CONFIG7
 * @n        Tap 检测门限配置
 */
constexpr uint8_t kApexConfig7 = 0x46;

/**
 * @brief Register: APEX_CONFIG8
 * @n        Tap 检测时间窗口配置
 */
constexpr uint8_t kApexConfig8 = 0x47;

/**
 * @brief Register: APEX_DATA4
 * @n        Tap 信息读取
 */
constexpr uint8_t kApexData4 = 0x35;

/**
 * @brief Register: ACCEL_WOM_X_THR
 * @n        X 轴 WOM 阈值
 */
constexpr uint8_t kAccelWomXThr = 0x4A;

/**
 * @brief Register: ACCEL_WOM_Y_THR
 * @n        Y 轴 WOM 阈值
 */
constexpr uint8_t kAccelWomYThr = 0x4B;

/**
 * @brief Register: ACCEL_WOM_Z_THR
 * @n        Z 轴 WOM 阈值
 */
constexpr uint8_t kAccelWomZThr = 0x4C;

/**
 * @brief Register: FIFO_CONFIG
 * @n        FIFO 启停控制
 */
constexpr uint8_t kFifoConfig = 0x16;

/**
 * @brief Register: FIFO_CONFIG1
 * @n        FIFO 数据源配置
 */
constexpr uint8_t kFifoConfig1 = 0x5F;

/**
 * @brief Register: FIFO_DATA
 * @n        FIFO 数据读取
 */
constexpr uint8_t kFifoData = 0x30;
} // namespace imu42688p_reg

/**
 * @brief ICM42688P/IMU42688P I2C 驱动封装（基于 ESP-IDF I2C 主机驱动）
 */
class Imu42688p {
public:
    /**
     * @brief 轴枚举定义
     */
    enum Axis : uint8_t {
        AxisX = 0,
        AxisY = 2,
        AxisZ = 4,
        AxisAll = 5,
    };

    /**
     * @brief 敲击次数枚举
     */
    enum TapNum : uint8_t {
        TapNone = 0,
        TapSingle = 8,
        TapDouble = 16,
    };

    /**
     * @brief 三轴数据结构
     */
    struct Vec3f {
        float x;
        float y;
        float z;
    };

    /**
     * @brief 敲击信息
     */
    struct TapInfo {
        uint8_t num;
        uint8_t axis;
        uint8_t direction;
    };

    /**
     * @brief 构造函数
     */
    Imu42688p();

    /**
     * @brief 初始化函数
     * @param bus 已初始化的 I2C 主机总线
     * @param addr 7-bit I2C 地址
     * @return 初始化结果
     * @retval ESP_OK               初始化成功
     * @retval ESP_ERR_INVALID_ARG  参数错误
     * @retval ESP_ERR_INVALID_STATE 总线/设备状态错误
     * @retval ESP_ERR_INVALID_RESPONSE 读取的传感器 ID 有误
     */
    esp_err_t begin(hal_i2c_bus_t *bus, uint8_t addr, uint32_t i2c_hz);

    // 使用 begin(bus, addr, i2c_hz) 进行初始化，由上层传入地址与时钟

    /**
     * @brief 获取测量温度值
     * @param out_c 输出温度（单位：℃）
     * @return 读取结果
     */
    esp_err_t read_temperature(float *out_c);

    /**
     * @brief 获取三轴加速度
     * @param out_g 输出加速度（单位：mg）
     * @return 读取结果
     */
    esp_err_t read_accel(Vec3f *out_g);

    /**
     * @brief 获取三轴陀螺仪
     * @param out_dps 输出角速度（单位：dps）
     * @return 读取结果
     */
    esp_err_t read_gyro(Vec3f *out_dps);

    /**
     * @brief 一次读取温度、加速度、陀螺仪
     * @param out_g 输出加速度（单位：mg）
     * @param out_dps 输出角速度（单位：dps）
     * @param out_c 输出温度（单位：℃）
     * @return 读取结果
     */
    esp_err_t read_all(Vec3f *out_g, Vec3f *out_dps, float *out_c);

    /**
     * @brief 敲击事件初始化
     * @param low_power_mode true 低功耗模式，false 低噪声模式
     * @return 初始化结果
     */
    esp_err_t tap_init(bool low_power_mode);

    /**
     * @brief 获取敲击信息
     * @param info 敲击信息输出
     * @return 读取结果
     */
    esp_err_t get_tap_info(TapInfo *info);

    /**
     * @brief 初始化移动唤醒（WOM）
     * @return 初始化结果
     */
    esp_err_t wom_init(void);

    /**
     * @brief 设置某轴加速度计的运动唤醒阈值
     * @param axis X/Y/Z/ALL
     * @param threshold Range(0-255), 1g/256 ~= 3.9mg
     * @return 设置结果
     */
    esp_err_t set_wom_threshold(uint8_t axis, uint8_t threshold);

    /**
     * @brief 使能运动唤醒中断
     * @param axis X/Y/Z
     * @return 设置结果
     */
    esp_err_t enable_wom_interrupt(uint8_t axis);

    /**
     * @brief 启用 FIFO
     * @return 设置结果
     */
    esp_err_t fifo_start(void);

    /**
     * @brief 关闭 FIFO
     * @return 设置结果
     */
    esp_err_t fifo_stop(void);

    /**
     * @brief 读取 FIFO 数据（温度/陀螺仪/加速度）
     * @param out_g 输出加速度（单位：mg）
     * @param out_dps 输出角速度（单位：dps）
     * @param out_c 输出温度（单位：℃）
     * @return 读取结果
     */
    esp_err_t fifo_read(Vec3f *out_g, Vec3f *out_dps, float *out_c);

private:
    esp_err_t write_reg(uint8_t reg, const uint8_t *data, size_t len);
    esp_err_t read_reg(uint8_t reg, uint8_t *data, size_t len);
    esp_err_t write_reg_u8(uint8_t reg, uint8_t value);

    hal_i2c_device_t device_;
    bool initialized_;
    float gyro_range_;
    float accel_range_;
    uint8_t int_pin_;
    bool fifo_mode_;
};
