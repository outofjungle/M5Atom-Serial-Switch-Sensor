#pragma once

// Error codes returned by internal functions. proto_cmd.c turns these into
// the text codes in docs/07-error-codes.md when it builds an ERR response.
#define ERR_NOCH   (-1) // ENOCH: no such channel
#define ERR_ACCESS (-2) // EACCESS: wrong access mode for this channel
#define ERR_RANGE  (-3) // ERANGE: value out of range
#define ERR_STATE  (-4) // ESTATE: command does not apply right now
#define ERR_ARG    (-5) // EARG: bad argument
