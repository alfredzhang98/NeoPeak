idf_component_register(
    SRCS "buzzer/buzzer.cpp"
         "motor/motor.cpp"
         "imu/icm42688p.cpp"
         "audio/tone_player.cpp"
         "audio/tone_music.cpp"
         "audio/tone_utils.cpp"
    INCLUDE_DIRS "buzzer"
                 "motor"
                 "imu"
                 "audio"
    REQUIRES esp_driver_ledc hal_board esp_rom
)
