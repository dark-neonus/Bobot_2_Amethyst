idf_component_register(
    SRCS "button_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        driver
        esp_driver_gpio
        esp_driver_i2c
    )
