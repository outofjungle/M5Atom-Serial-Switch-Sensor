#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Handle one command frame's CBOR payload.
 *
 * Parses [0, seq, verb, arg, ...], runs the command, and sends the
 * response frame or frames through usb_cdc_send_response_frame(). See
 * docs/03-message-model.md and docs/05-command-reference.md.
 *
 * @param[in] payload A CBOR payload already checked by proto_frame_decode().
 * @param[in] len Number of bytes in payload.
 */
void proto_cmd_dispatch(const uint8_t *payload, size_t len);
