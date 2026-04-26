idf_component_register(
    SRCS "hal_input_power.cpp"
         "hal_encoder.cpp"
         "hal_i2c.cpp"
         "hal_pwm.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_ledc
)
