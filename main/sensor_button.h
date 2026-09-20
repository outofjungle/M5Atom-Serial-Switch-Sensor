#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Sets up the push button on G41. See docs/01-hardware-atoms3-lite.md.
esp_err_t sensor_button_init(void);

// Returns true if the button is pressed right now.
bool sensor_button_is_pressed(void);

// Returns the number of presses counted since startup.
uint32_t sensor_button_press_count(void);

// Returns the timestamp, in microseconds since startup, of the last time
// the button's state changed.
uint64_t sensor_button_last_edge_us(void);
