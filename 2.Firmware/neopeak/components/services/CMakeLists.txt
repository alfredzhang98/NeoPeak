idf_component_register(
    SRCS "core/tone_player.cpp"
        "core/tone_music.cpp"
        "core/message_bus.cpp"
            "core/tone_utils.cpp"
        "imu/imu_service.cpp"
        "power/power_service.cpp"
            "comm/encoder_service.cpp"
    INCLUDE_DIRS "."
              "core"
              "imu"
              "power"
              "display"
              "comm"
    REQUIRES drivers hal_board esp_psram
)
