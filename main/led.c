#include "led.h"
#include "led_strip.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#define LED_GPIO 35

static const char *TAG = "led";
static led_strip_handle_t s_strip;
static uint8_t s_last_rgb[3] = {0, 0, 0};

// led_set_rgb runs on the RX task (SET led0). led_get_rgb runs on the RX
// task (GET led0) or on the esp_timer sampler task, if a host ever
// subscribes led0 for streaming. A lock keeps a concurrent read from
// seeing a mix of an old and a new color.
static portMUX_TYPE s_rgb_mux = portMUX_INITIALIZER_UNLOCKED;

esp_err_t led_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_new_rmt_device failed: %d", err);
        return err;
    }

    return led_strip_clear(s_strip);
}

esp_err_t led_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    esp_err_t err = led_strip_set_pixel(s_strip, 0, r, g, b);
    if (err != ESP_OK) {
        return err;
    }
    err = led_strip_refresh(s_strip);
    if (err != ESP_OK) {
        return err;
    }
    portENTER_CRITICAL(&s_rgb_mux);
    s_last_rgb[0] = r;
    s_last_rgb[1] = g;
    s_last_rgb[2] = b;
    portEXIT_CRITICAL(&s_rgb_mux);
    return ESP_OK;
}

void led_get_rgb(uint8_t out[3])
{
    portENTER_CRITICAL(&s_rgb_mux);
    out[0] = s_last_rgb[0];
    out[1] = s_last_rgb[1];
    out[2] = s_last_rgb[2];
    portEXIT_CRITICAL(&s_rgb_mux);
}
