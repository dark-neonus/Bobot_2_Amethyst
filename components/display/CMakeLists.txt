idf_component_register(
    SRCS "display.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        u8g2-hal-esp-idf
        esp_driver_gpio
        driver
    )
