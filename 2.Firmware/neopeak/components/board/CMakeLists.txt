idf_component_register(
    SRCS "board_init.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES drivers services
    REQUIRES esp_psram
)
