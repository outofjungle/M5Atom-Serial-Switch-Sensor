#include "channels.h"
#include "sensor_button.h"
#include "led.h"
#include "esp_timer.h"
#include <string.h>

static esp_err_t cbor_to_esp(CborError e)
{
    return (e == CborNoError) ? ESP_OK : ESP_FAIL;
}

static esp_err_t get_btn0(CborEncoder *enc)
{
    return cbor_to_esp(cbor_encode_boolean(enc, sensor_button_is_pressed()));
}

static esp_err_t get_btn0_cnt(CborEncoder *enc)
{
    return cbor_to_esp(cbor_encode_uint(enc, sensor_button_press_count()));
}

static esp_err_t get_btn0_us(CborEncoder *enc)
{
    return cbor_to_esp(cbor_encode_uint(enc, sensor_button_last_edge_us()));
}

static esp_err_t get_uptime_us(CborEncoder *enc)
{
    return cbor_to_esp(cbor_encode_uint(enc, (uint64_t)esp_timer_get_time()));
}

static esp_err_t get_led0(CborEncoder *enc)
{
    uint8_t rgb[3];
    led_get_rgb(rgb);
    return cbor_to_esp(cbor_encode_byte_string(enc, rgb, sizeof(rgb)));
}

static int set_led0(CborValue *val)
{
    if (!cbor_value_is_byte_string(val)) {
        return ERR_RANGE;
    }
    uint8_t rgb[3];
    size_t len = sizeof(rgb);
    if (cbor_value_copy_byte_string(val, rgb, &len, NULL) != CborNoError || len != 3) {
        return ERR_RANGE;
    }
    return (led_set_rgb(rgb[0], rgb[1], rgb[2]) == ESP_OK) ? 0 : ERR_RANGE;
}

static const channel_def_t s_channels[] = {
    { "btn0", "bool", "RO", get_btn0, NULL },
    { "btn0_cnt", "uint", "RO", get_btn0_cnt, NULL },
    { "btn0_us", "uint", "RO", get_btn0_us, NULL },
    { "led0", "bytes", "RW", get_led0, set_led0 },
    { "uptime_us", "uint", "RO", get_uptime_us, NULL },
};

#define CHANNEL_COUNT (sizeof(s_channels) / sizeof(s_channels[0]))

size_t channels_count(void)
{
    return CHANNEL_COUNT;
}

const channel_def_t *channels_at(size_t index)
{
    return &s_channels[index];
}

const channel_def_t *channels_find(const char *name)
{
    for (size_t i = 0; i < CHANNEL_COUNT; i++) {
        if (strcmp(s_channels[i].name, name) == 0) {
            return &s_channels[i];
        }
    }
    return NULL;
}
