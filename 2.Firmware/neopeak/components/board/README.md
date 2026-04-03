# board

## 目的
板级抽象层（Board Support），对硬件资源进行统一封装与配置，向上屏蔽板卡差异。

## 这里放什么
- 板卡初始化流程与默认配置
- 引脚映射、外设复用配置
- 板级资源表（GPIO、总线、供电、时钟）
- 统一管理 CONFIG_ 并向 drivers/services 传递配置

## 开发指导
- 不直接访问寄存器或具体外设驱动，避免与 drivers/hal_board 重复。
- 只做与“板子”相关的差异化处理。
- 对外暴露清晰的初始化与资源查询接口。

## 示例
- board_init()
- board_init_*_configs()
- board_get_display_pins()
