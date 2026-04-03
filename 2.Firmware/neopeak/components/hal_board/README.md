# HAL (hal_board)

## 目的
硬件抽象层（Hardware Abstraction Layer），对底层外设访问进行再抽象，向上提供统一接口。

## 换板子提示
当需要更换板子时，优先只修改 HAL 的实现（.cpp）。
上层 services/board 依赖 HAL 接口，不应该直接依赖底层 SDK 或寄存器。
保持 HAL 接口稳定，可以让业务逻辑在不同板子间复用。

## 这里放什么
- 面向功能的硬件抽象接口（如 hal_i2c/hal_spi/hal_gpio）
- 统一的设备访问模型（初始化、读写、错误码）

## 开发指导
- 向下依赖底层 SDK，向上被 board/services 使用。
- 不做业务逻辑，仅提供稳定硬件能力。
- 保持接口稳定与可移植。

## 示例
- hal_i2c_master_bus_init()
- hal_i2c_master_device_write_read()
- hal_gpio_set_level()
- hal_input_power_init()
- hal_encoder_init()
- hal_pwm_timer_init()
