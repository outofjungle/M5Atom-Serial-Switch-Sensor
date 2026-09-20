#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_app_desc.h"

#include "app_config.h"
#include "sensor_button.h"
#include "led.h"
#include "usb_cdc.h"
#include "app_stream.h"

static const char *TAG = "app_main";

static void init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void app_main(void)
{
    init_nvs();
    ESP_ERROR_CHECK(app_config_init());
    ESP_ERROR_CHECK(led_init());
    ESP_ERROR_CHECK(sensor_button_init());

    // Holding the button through boot enables the debug (log) port,
    // CDC1. Read right after sensor_button_init() sets the true baseline
    // level, so this is a clean, correct snapshot of the button at boot
    // -- not an edge, so it does not affect btn0_cnt. See
    // docs/01-hardware-atoms3-lite.md.
    bool enable_debug_port = sensor_button_is_pressed();
    ESP_ERROR_CHECK(usb_cdc_init(enable_debug_port));
    ESP_ERROR_CHECK(app_stream_init());

    const esp_app_desc_t *desc = esp_app_get_description();
    ESP_LOGI(TAG, "M5Atom Serial Switch Sensor, firmware %s, serial %s, debug port %s",
             desc->version, usb_cdc_get_serial(), enable_debug_port ? "on" : "off");
}
