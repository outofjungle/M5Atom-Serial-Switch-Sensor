#pragma once

// Shared constants for the frame protocol. See docs/03-message-model.md and
// docs/04-cbor-encoding.md.

// Frame kinds. The first item of every frame's CBOR array.
#define FRAME_KIND_CMD  0
#define FRAME_KIND_OK   1
#define FRAME_KIND_ERR  2
#define FRAME_KIND_ROW  3
#define FRAME_KIND_DATA 4

// A full wire frame, including COBS overhead and the trailing 0x00
// delimiter, does not exceed this many bytes. See docs/04-cbor-encoding.md.
#define PROTO_MAX_WIRE_FRAME 256

// The largest CBOR payload that can still fit inside PROTO_MAX_WIRE_FRAME
// after the CRC (2 bytes), COBS overhead (at most 1 byte for a frame this
// small), and the delimiter (1 byte) are added.
#define PROTO_MAX_PAYLOAD (PROTO_MAX_WIRE_FRAME - 2 - 1 - 1)

// A little more than PROTO_MAX_WIRE_FRAME, so a moderately oversized but
// otherwise well-formed incoming frame can still be decoded far enough to
// recover a trustworthy sequence number and answer with EOVF (see
// docs/07-error-codes.md), instead of being silently dropped as
// unreadable. Used both for the receive accumulator in usb_cdc.c and for
// proto_frame_decode()'s internal scratch buffer in proto_frame.c -- both
// must agree, or a frame this large will decode in one place and fail in
// the other.
#define PROTO_DECODE_CAP (PROTO_MAX_WIRE_FRAME + 64)

// Flags bits carried in a sample frame. See docs/06-channel-model-and-types.md.
#define FLAG_DROP  (1u << 0)
#define FLAG_STALE (1u << 1)
#define FLAG_SAT   (1u << 2)
