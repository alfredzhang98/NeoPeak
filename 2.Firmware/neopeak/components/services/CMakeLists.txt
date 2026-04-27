idf_component_register(
    SRCS "core/message_bus.cpp"
         "power/power_service.cpp"
         "input/encoder_service.cpp"
         "feedback/feedback_service.cpp"
         "imu/imu_service.cpp"
         "display/display_service.cpp"
    INCLUDE_DIRS "."
                 "core"
                 "power"
                 "input"
                 "feedback"
                 "imu"
                 "display"
    REQUIRES drivers hal_board ui esp_psram esp_driver_spi esp_lcd esp_driver_gpio esp_lvgl_port
)
