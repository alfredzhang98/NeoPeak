# NeoPeak Firmware

ESP-IDF firmware for the NeoPeak board. Uses board/HAL/drivers/services layering with a message bus for data flow.

## Build and flash
- ESP-IDF: 5.5.2
- Configure pins and options in Kconfig (`menuconfig`).
- Build/flash using standard ESP-IDF workflow.

## Structure
- main: entry point (`app_main`)
- components/board: board_init and board-level wiring
- components/hal_board: hardware abstraction (replace for new boards)
- components/drivers: device drivers built on HAL
- components/services: app logic and data flow (message bus, IMU, power, encoder)

## Notes
- `board_init()` centralizes all CONFIG_ usage and passes configs into drivers/services.
- Services communicate via `message_bus` topics.
