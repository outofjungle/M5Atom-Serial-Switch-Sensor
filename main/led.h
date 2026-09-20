#pragma once

#include <stdint.h>
#include "esp_err.h"

// Controls the onboard WS2812C RGB LED on G35. See
// docs/01-hardware-atoms3-lite.md.

esp_err_t led_init(void);

// Sets the LED color. r, g, b are 0-255.
esp_err_t led_set_rgb(uint8_t r, uint8_t g, uint8_t b);

// Returns the last color set with led_set_rgb, through out[0..2] as r, g, b.
// The hardware cannot be read back, so this returns the last value this
// firmware wrote.
void led_get_rgb(uint8_t out[3]);
