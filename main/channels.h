#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "cbor.h"
#include "err_codes.h"

typedef struct {
    const char *name;
    // The type name shown in a CAPS row. See
    // docs/06-channel-model-and-types.md.
    const char *type_name;
    // "RO", "WO", or "RW".
    const char *access;
    // Encodes the channel's current value as one CBOR item into enc.
    esp_err_t (*get)(CborEncoder *enc);
    // Decodes and applies a new value from val. NULL for a read-only
    // channel.
    int (*set)(CborValue *val);
} channel_def_t;

// Returns the number of channels in the registry.
size_t channels_count(void);

// Returns the channel at this position, in registry order. index must be
// less than channels_count().
const channel_def_t *channels_at(size_t index);

// Finds a channel by name. Returns NULL if no channel has this name.
const channel_def_t *channels_find(const char *name);
