#include "ultrasonic.h"
#include "usb_cdc.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Header pins G39 (Trig) and G38 (Echo). See docs/01-hardware-atoms3-lite.md.
#define TRIG_GPIO GPIO_NUM_39
#define ECHO_GPIO GPIO_NUM_38

// How this channel conditions its signal: fixed timing constants tuned to
// this specific sensor, owned entirely by this file. Never a command
// argument or a CFG key. See docs/12-hardware-abstraction.md. Values below
// come from the sensor's own datasheet: docs/KS0504-ultrasonic-sensor-datasheet.pdf.
//
// US_TRIG_PULSE_US: the sensor's datasheet minimum trigger pulse width
// (10us TTL pulse).
#define US_TRIG_PULSE_US 10
// US_ECHO_TIMEOUT_US: round-trip time of sound over the sensor's stated
// 3-meter maximum range (about 17,500us at 343 m/s), plus margin. A real
// HC-SR04-family sensor with nothing in range never raises Echo at all,
// so this timeout is what turns "no echo" into a bounded wait rather than
// an indefinite one.
#define US_ECHO_TIMEOUT_US 30000
// US_RETRIGGER_GUARD_US: minimum time between the end of one ranging
// cycle and the start of the next, so ultrasonic ringing from one pulse
// has settled. Matches the 50ms delay the datasheet's own reference code
// uses between readings.
#define US_RETRIGGER_GUARD_US 50000

// Sensor's stated valid range: "Max Range: 3m", "Min Range: less than
// 4cm". A computed distance outside this range is treated the same as a
// timeout: reported as no reading. See docs/06-channel-model-and-types.md.
#define US_MIN_RANGE_MM 40
#define US_MAX_RANGE_MM 3000

// Millimeters per microsecond of round-trip time, at 343 m/s, as a
// fixed-point fraction (343/2000 == 0.1715), to avoid floating point in
// the ISR-adjacent measurement path.
#define US_MM_PER_US_NUM 343
#define US_MM_PER_US_DEN 2000

static const char *TAG = "ultrasonic";

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool s_expecting = false;
static volatile uint64_t s_rise_us = 0;
static volatile uint64_t s_pulse_us = 0;

static SemaphoreHandle_t s_echo_sem;
static SemaphoreHandle_t s_meas_mutex;
static uint64_t s_last_cycle_end_us = 0;

static void IRAM_ATTR echo_isr_handler(void *arg)
{
    (void)arg;
    int64_t now_us = esp_timer_get_time();
    bool level = gpio_get_level(ECHO_GPIO) != 0;

    portENTER_CRITICAL_ISR(&s_mux);
    if (!s_expecting) {
        portEXIT_CRITICAL_ISR(&s_mux);
        return;
    }
    if (level) {
        // Rising edge: the echo pulse starts.
        s_rise_us = (uint64_t)now_us;
        portEXIT_CRITICAL_ISR(&s_mux);
        return;
    }
    // Falling edge: the echo pulse ends.
    s_pulse_us = (uint64_t)now_us - s_rise_us;
    s_expecting = false;
    portEXIT_CRITICAL_ISR(&s_mux);

    BaseType_t higher_prio_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_echo_sem, &higher_prio_woken);
    if (higher_prio_woken) {
        portYIELD_FROM_ISR();
    }
}

esp_err_t ultrasonic_init(void)
{
    gpio_config_t trig_conf = {
        .pin_bit_mask = (1ULL << TRIG_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&trig_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config(TRIG) failed: %d", err);
        return err;
    }
    gpio_set_level(TRIG_GPIO, 0);

    gpio_config_t echo_conf = {
        .pin_bit_mask = (1ULL << ECHO_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    err = gpio_config(&echo_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config(ECHO) failed: %d", err);
        return err;
    }

    s_echo_sem = xSemaphoreCreateBinary();
    s_meas_mutex = xSemaphoreCreateMutex();
    if (s_echo_sem == NULL || s_meas_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        // ESP_ERR_INVALID_STATE means another driver already installed the
        // shared ISR service (sensor_button.c, in this project). That is
        // fine.
        ESP_LOGE(TAG, "gpio_install_isr_service failed: %d", err);
        return err;
    }

    err = gpio_isr_handler_add(ECHO_GPIO, echo_isr_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_isr_handler_add failed: %d", err);
        return err;
    }

    return ESP_OK;
}

esp_err_t ultrasonic_measure_mm(uint32_t *out_mm, bool *out_valid)
{
    xSemaphoreTake(s_meas_mutex, portMAX_DELAY);

    // Enforce the retrigger guard so ultrasonic ringing from the previous
    // cycle has settled. See docs/12-hardware-abstraction.md.
    if (s_last_cycle_end_us != 0) {
        int64_t elapsed = esp_timer_get_time() - (int64_t)s_last_cycle_end_us;
        if (elapsed < US_RETRIGGER_GUARD_US) {
            vTaskDelay(pdMS_TO_TICKS((US_RETRIGGER_GUARD_US - elapsed) / 1000 + 1));
        }
    }

    // Clear any stale signal left over from a previous cycle that timed
    // out after its falling edge finally arrived.
    xSemaphoreTake(s_echo_sem, 0);

    portENTER_CRITICAL(&s_mux);
    s_expecting = true;
    portEXIT_CRITICAL(&s_mux);

    gpio_set_level(TRIG_GPIO, 1);
    esp_rom_delay_us(US_TRIG_PULSE_US);
    gpio_set_level(TRIG_GPIO, 0);

    bool got_echo = xSemaphoreTake(s_echo_sem, pdMS_TO_TICKS(US_ECHO_TIMEOUT_US / 1000 + 5)) == pdTRUE;

    portENTER_CRITICAL(&s_mux);
    s_expecting = false; // stop accepting edges, whether or not this timed out
    uint64_t pulse_us = s_pulse_us;
    portEXIT_CRITICAL(&s_mux);

    s_last_cycle_end_us = (uint64_t)esp_timer_get_time();

    bool valid = false;
    uint32_t mm = 0;
    if (got_echo) {
        uint64_t d = (pulse_us * US_MM_PER_US_NUM) / US_MM_PER_US_DEN;
        if (d >= US_MIN_RANGE_MM && d <= US_MAX_RANGE_MM) {
            mm = (uint32_t)d;
            valid = true;
        }
    } else {
        usb_cdc_note_ranging_timeout();
    }

    xSemaphoreGive(s_meas_mutex);

    *out_mm = mm;
    *out_valid = valid;
    return ESP_OK;
}
