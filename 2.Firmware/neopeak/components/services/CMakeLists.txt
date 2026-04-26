idf_component_register(
    SRCS "core/message_bus.cpp"
         "power/power_service.cpp"
         "input/encoder_service.cpp"
         "feedback/feedback_service.cpp"
    INCLUDE_DIRS "."
                 "core"
                 "power"
                 "input"
                 "feedback"
    REQUIRES drivers hal_board esp_psram
)
