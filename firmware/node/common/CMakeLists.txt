idf_component_register(
    SRCS        "nh_node.c"
                "nh_kurulum.c"
                "nh_modbus.c"
                "nh_kayit.c"
                "port/nh_port_esp32.c"
    INCLUDE_DIRS "include"
    REQUIRES     contract driver esp_timer nvs_flash esp_hw_support freertos
)
