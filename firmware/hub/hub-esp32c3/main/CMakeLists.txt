idf_component_register(
    SRCS "app_main.c" "nh_hub_kurulum.c" "nh_ayarlar.c" "nh_wifi.c" "nh_web.c" "nh_tipler.c" "nh_olcum.c"
    PRIV_REQUIRES
        esp-tls
        esp_wifi
        esp_netif
        esp_event
        esp_hw_support
        esp_http_server
        json
        nvs_flash
        esp_driver_gpio
    REQUIRES contract driver
    INCLUDE_DIRS "."
    EMBED_TXTFILES "kurulum.html"
)
