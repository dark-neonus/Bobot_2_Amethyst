idf_component_register(
    SRCS "asset_uploader.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_http_server
        esp_wifi
        esp_netif
        nvs_flash
        sd_card
)
