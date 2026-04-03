idf_component_register(
    SRCS "buzzer/buzzer.cpp"
         "motor/motor.cpp"
         "imu42688p/imu42688p.cpp"
    INCLUDE_DIRS "buzzer"
                 "motor"
                 "imu42688p"
    REQUIRES driver hal_board
)
