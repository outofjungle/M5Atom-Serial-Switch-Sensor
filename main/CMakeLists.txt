idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        tinycbor
        esp_tinyusb
        led_strip
        nvs_flash
        driver
        esp_timer
        app_update
)
