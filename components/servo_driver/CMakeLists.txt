idf_component_register(
    SRCS "servo_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_i2c
)
