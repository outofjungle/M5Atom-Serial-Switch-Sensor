# CBOR Encoding

This document describes how a frame, as defined in `docs/03-message-model.md`, is written as
bytes on the wire. See `docs/00-overview.md` for term definitions.

Three steps turn a frame's array content into wire bytes:

1. Encode the array as Concise Binary Object Representation (CBOR) bytes. This is the payload.
2. Compute a Cyclic Redundancy Check (CRC) over the payload, and add it to the end.
3. Encode the result using Consistent Overhead Byte Stuffing (COBS), and add a delimiter byte.

## Step 1: CBOR encoding of the payload

CBOR is a binary data format. Every value's bytes state their own type. This project uses CBOR's
standard, unmodified rules. A programmer does not write a custom CBOR encoder by hand. A library
does this. See `docs/09-host-integration.md` for library names.

## Step 2: CRC

This project uses CRC-16/CCITT-FALSE.

| Parameter | Value |
|---|---|
| Polynomial | `0x1021` |
| Initial value | `0xFFFF` |
| Input reflected | No |
| Output reflected | No |
| Final XOR value | `0x0000` |

The CRC is computed over the CBOR payload bytes from step 1, using this algorithm. The result is
a 16-bit number. It is added to the end of the payload as 2 bytes, most significant byte first.

The result of this step is called the framed payload: the CBOR payload, followed by its 2-byte
CRC.

### Computing the same CRC on a host computer

A host program written in Python can compute the same CRC using the standard library, with no
extra software installed:

```python
from binascii import crc_hqx

crc = crc_hqx(payload_bytes, 0xFFFF)
```

### Handling a CRC failure

If a received frame's computed CRC does not match the CRC carried in that frame, the reader
discards the frame. The device responds with error `ECRC`. See `docs/07-error-codes.md` and
`docs/11-test-plan.md`.

## Step 3: COBS and the frame delimiter

A CBOR payload can contain any byte value, including the byte value `0x00`. This project needs
one byte value that always marks the end of a frame, the way a line feed character marked the end
of a line in an earlier, text-based design considered for this project. COBS makes this possible.

COBS re-writes the framed payload from step 2 so that the byte value `0x00` never appears inside
the result. The value `0x00` is then always safe to use as a frame delimiter.

1. Encode the framed payload using COBS. This adds a small number of extra bytes: usually 1 byte,
   for any frame under 254 bytes.
2. Add one `0x00` byte after the COBS-encoded bytes. This byte is the frame delimiter.

A reader does this in reverse:

1. Read bytes until a `0x00` byte is found. Everything before it is one COBS-encoded frame.
2. Decode those bytes using COBS. This recovers the framed payload from step 2.
3. Split the last 2 bytes off as the received CRC. Compute the CRC over the remaining bytes.
   Compare the two. If they do not match, discard the frame and report `ECRC`.
4. If the CRC matches, decode the remaining bytes as CBOR. This recovers the array described in
   `docs/03-message-model.md`.

A programmer does not write a custom COBS encoder or decoder by hand in most cases. A small,
existing implementation is used instead. See `docs/09-host-integration.md` for library names.

## Frame length limit

A full wire frame, including the COBS overhead and the trailing `0x00` delimiter, does not exceed
256 bytes. The device discards a longer incoming frame and reports error `EOVF`. See
`docs/07-error-codes.md`.

## Resynchronizing after a bad frame

If a reader finds bytes that do not decode correctly as COBS, or that decode but fail the CRC
check, or that decode and pass the CRC check but fail to parse as CBOR, the reader discards those
bytes. The reader then looks for the next `0x00` byte and resumes normal reading from there. This
lets the system recover from one corrupted frame without a restart.

## Worked example: a PING command

The command frame's content, from `docs/03-message-model.md`, is the array `[0, 1, "PING"]`. This
table shows every step, with real, computed byte values.

| Step | Bytes (hexadecimal) | Length |
|---|---|---|
| 1. CBOR payload | `83 00 01 64 50 49 4E 47` | 8 bytes |
| 2. Framed payload (payload + CRC `3157`) | `83 00 01 64 50 49 4E 47 31 57` | 10 bytes |
| 3. COBS-encoded | `02 83 09 01 64 50 49 4E 47 31 57` | 11 bytes |
| 3. Full wire frame (with trailing `00`) | `02 83 09 01 64 50 49 4E 47 31 57 00` | 12 bytes |

Reading the CBOR payload byte by byte:

| Bytes | Meaning |
|---|---|
| `83` | An array of 3 items follows. |
| `00` | Unsigned integer `0` — the kind, `CMD`. |
| `01` | Unsigned integer `1` — the sequence number. |
| `64 50 49 4E 47` | A text string of length 4: `"PING"`. |

## Worked example: a sample frame

The sample frame's content, from `docs/03-message-model.md`, is the array
`[4, 66, 123999, "btn0", true, 0]`. This is the `DATA` kind, sequence number 66, timestamp
123999 microseconds, channel `btn0`, value `true`, flags `0`.

| Step | Bytes (hexadecimal) | Length |
|---|---|---|
| 1. CBOR payload | `86 04 18 42 1A 00 01 E4 5F 64 62 74 6E 30 F5 00` | 16 bytes |
| 2. Framed payload (payload + CRC `A86C`) | `86 04 18 42 1A 00 01 E4 5F 64 62 74 6E 30 F5 00 A8 6C` | 18 bytes |
| 3. COBS-encoded | `06 86 04 18 42 1A 0A 01 E4 5F 64 62 74 6E 30 F5 03 A8 6C` | 19 bytes |
| 3. Full wire frame (with trailing `00`) | `06 86 04 18 42 1A 0A 01 E4 5F 64 62 74 6E 30 F5 03 A8 6C 00` | 20 bytes |

This example is 20 bytes on the wire, compared to about 30 bytes for the earlier, text-based
design considered for this project. `docs/08-flow-control-and-backpressure.md` uses this frame
size in its buffer-filling estimates.
