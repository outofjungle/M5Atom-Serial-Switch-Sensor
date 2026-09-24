#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Drives the ultrasonic distance sensor on G39 (Trig) and G38 (Echo). See
// docs/01-hardware-atoms3-lite.md and docs/12-hardware-abstraction.md.

// Sets up the Trig output and the Echo input's edge-timestamp interrupt.
esp_err_t ultrasonic_init(void);

// Runs one complete ranging cycle: drives one trigger pulse, waits for the
// echo, and computes distance. Blocks the caller for up to the ranging
// timeout plus any retrigger guard wait still owed from the previous
// cycle -- tens of milliseconds in the worst case. Safe to call from more
// than one task; calls serialize against each other.
//
// @param[out] out_mm Distance in millimeters. Only meaningful if
//     *out_valid is true.
// @param[out] out_valid true if an echo was detected within the timeout
//     and the computed distance fell inside the sensor's stated range.
//     false otherwise -- the caller should report this reading as the
//     CBOR value null. See docs/06-channel-model-and-types.md.
esp_err_t ultrasonic_measure_mm(uint32_t *out_mm, bool *out_valid);
