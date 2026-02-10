idf_component_register(SRCS "web_server.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_http_server esp_wifi esp_netif nvs_flash servo_driver kinematic)