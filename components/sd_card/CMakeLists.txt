idf_component_register(
    SRCS "sd_card.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        driver
        esp_driver_gpio
        fatfs
        sdmmc
    )
