# drivers

## 目的
设备驱动层，直接操作外设或器件，提供稳定的设备访问接口。

## 这里放什么
- SPI/I2C/UART/GPIO/PWM 等外设驱动
- 具体器件驱动（传感器、屏幕、编码器等）

## 开发指导
- 封装底层 SDK/寄存器细节，对外暴露简洁 API。
- 通过 HAL 访问 I2C/PWM/GPIO 等能力，避免与板级逻辑耦合。
- 不包含业务逻辑，保持可复用与可测试。
- 需要时提供 mock 或测试桩。

## 示例
- imu42688p_begin()
- imu42688p_read_raw()
- buzzer_start()
- motor_pwm_shake()
