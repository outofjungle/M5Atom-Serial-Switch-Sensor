#pragma once

#include "esp_err.h"

// Runs the periodic sampler that sends DATA frames for subscribed
// channels while streaming is on. See
// docs/05-command-reference.md, "Streaming commands".

// Starts the periodic timer. Call once, after app_config_init().
esp_err_t app_stream_init(void);

// Applies the current PERIOD_US value to the running timer. Call this
// after app_config_set_period_us() changes the value.
void app_stream_apply_period(void);
