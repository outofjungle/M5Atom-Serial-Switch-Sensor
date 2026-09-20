#include "sensor_button.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#define BUTTON_GPIO GPIO_NUM_41

// How this channel conditions its signal: a fixed time window since the
// last accepted edge, tuned for a mechanical tactile switch. This is a
// device-internal decision, not something the wire protocol exposes --
// see docs/12-hardware-abstraction.md. A future channel wired to a
// sensor that switches deterministically would need no such window at
// all; that is also this module's decision to make, not the host's.
#define BTN_DEBOUNCE_US 20000

static const char *TAG = "sensor_button";

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool s_pressed = false;
static volatile uint32_t s_press_count = 0;
static volatile uint64_t s_last_edge_us = 0;

static void IRAM_ATTR button_isr_handler(void *arg)
{
    (void)arg;
    int64_t now_us = esp_timer_get_time();
    bool new_pressed = (gpio_get_level(BUTTON_GPIO) == 0); // active low

    portENTER_CRITICAL_ISR(&s_mux);
    if ((uint64_t)now_us - s_last_edge_us >= BTN_DEBOUNCE_US && new_pressed != s_pressed) {
        s_pressed = new_pressed;
        s_last_edge_us = (uint64_t)now_us;
        if (new_pressed) {
            s_press_count++;
        }
    }
    portEXIT_CRITICAL_ISR(&s_mux);
}

esp_err_t sensor_button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %d", err);
        return err;
    }

    // Set the real baseline before the ISR can possibly fire. Both fields
    // are zero-initialized statics; if the ISR were armed first and fired
    // in the window before this runs (electrical noise, or the natural
    // transition when the pull-up first engages), it would compute
    // now_us - 0 >= BTN_DEBOUNCE_US -- true almost immediately -- and
    // could register a phantom press before the button was ever touched.
    s_pressed = (gpio_get_level(BUTTON_GPIO) == 0);
    s_last_edge_us = (uint64_t)esp_timer_get_time();

    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        // ESP_ERR_INVALID_STATE means another driver already installed the
        // shared ISR service. That is fine.
        ESP_LOGE(TAG, "gpio_install_isr_service failed: %d", err);
        return err;
    }

    err = gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_isr_handler_add failed: %d", err);
        return err;
    }

    return ESP_OK;
}

bool sensor_button_is_pressed(void)
{
    portENTER_CRITICAL(&s_mux);
    bool v = s_pressed;
    portEXIT_CRITICAL(&s_mux);
    return v;
}

uint32_t sensor_button_press_count(void)
{
    portENTER_CRITICAL(&s_mux);
    uint32_t v = s_press_count;
    portEXIT_CRITICAL(&s_mux);
    return v;
}

uint64_t sensor_button_last_edge_us(void)
{
    portENTER_CRITICAL(&s_mux);
    uint64_t v = s_last_edge_us;
    portEXIT_CRITICAL(&s_mux);
    return v;
}
