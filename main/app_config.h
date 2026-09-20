#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// Streaming configuration and the subscription list. See
// docs/05-command-reference.md, the "Streaming commands" section, and
// docs/08-flow-control-and-backpressure.md.

// Loads defaults, then loads saved values from NVS if any exist.
esp_err_t app_config_init(void);

// PERIOD_US: time between samples, in microseconds. 1000 or more.
uint32_t app_config_get_period_us(void);
int app_config_set_period_us(uint32_t value);

// MODE: "PERIODIC", "ONCHANGE", or "BOTH".
const char *app_config_get_mode(void);
int app_config_set_mode(const char *value);

// BATCH: number of sample frames the device groups into one USB write,
// from 1 to 32. This does not change the shape of a sample frame. It only
// changes how often the device flushes its USB transmit buffer.
uint16_t app_config_get_batch(void);
int app_config_set_batch(uint16_t value);

// ENCODING: always "CBOR" in this version. See docs/03-message-model.md.
const char *app_config_get_encoding(void);

// The subscription list.
int app_config_subscribe(const char *channel_name);
int app_config_unsubscribe(const char *channel_name);
bool app_config_is_subscribed(const char *channel_name);
size_t app_config_sub_count(void);
const char *app_config_sub_at(size_t index);

// Streaming state.
bool app_config_is_streaming(void);
int app_config_start_streaming(void);
int app_config_stop_streaming(void);

// Saves the current configuration and subscription list to NVS.
esp_err_t app_config_save(void);

// Loads the configuration and subscription list from NVS, replacing the
// current values. Returns ESP_ERR_NVS_NOT_FOUND if nothing was saved.
esp_err_t app_config_load(void);
