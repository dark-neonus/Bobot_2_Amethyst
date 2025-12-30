idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio
        esp_driver_i2c
        freertos
        esp_system
        u8g2-hal-esp-idf
        display
        button_driver
    PRIV_REQUIRES
        driver
        esp_timer
    )
