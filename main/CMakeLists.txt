idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio
        freertos
        esp_system
        u8g2-hal-esp-idf
    PRIV_REQUIRES
        driver
        esp_timer
    )
