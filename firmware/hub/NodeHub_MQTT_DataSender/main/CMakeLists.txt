idf_component_register(
    SRCS "app_main.c" "nh_hub_kurulum.c"
    PRIV_REQUIRES esp-tls esp_wifi esp_event nvs_flash esp_driver_gpio
    REQUIRES contract driver
    INCLUDE_DIRS "."
    EMBED_TXTFILES "mosquitto.org.crt"
)
