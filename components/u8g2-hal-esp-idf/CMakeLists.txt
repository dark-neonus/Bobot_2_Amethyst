file(GLOB_RECURSE SOURCES src/*.c)
idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS "include"
                       REQUIRES driver freertos u8g2 esp_driver_gpio esp_driver_spi esp_driver_i2c)