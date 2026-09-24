# Channel Model and Types

This document describes how a channel is named, typed, and listed. See `docs/00-overview.md` for
term definitions.

## What a channel is

A channel is one named source of data or one named control on the device. A channel has:

1. A name, such as `btn0`.
2. A declared type, such as `bool` or `uint`. See "Value types" below.
3. An access mode: read-only (`RO`), write-only (`WO`), or read and write (`RW`).

The `CAPS` command, described in `docs/05-command-reference.md`, lists every channel and its
declared type and access mode.

A channel's value is always clean, finished state. Any hardware-specific work needed to produce
that state — filtering a noisy signal, debouncing a mechanical switch — is the device's job, done
before the value ever reaches this model, and is never itself a configurable part of the
protocol. See `docs/12-hardware-abstraction.md`.

## Value types are native CBOR types

This project uses Concise Binary Object Representation (CBOR) to encode every value. See
`docs/04-cbor-encoding.md`. A CBOR value's bytes already state its own type. This table names
every type this project uses, and states which CBOR type it maps to.

| Type name | CBOR type | Meaning | Example |
|---|---|---|---|
| `bool` | Boolean | True or false | `true` |
| `uint` | Unsigned integer | A whole number, 0 or more | `4095` |
| `int` | Signed integer | A whole number, positive or negative | `-273` |
| `float` | Floating-point number | A number that can carry a fraction | `3.2741` |
| `text` | Text string | Readable text, in UTF-8 | `"btn0"` |
| `bytes` | Byte string | Raw bytes, of stated meaning | 3 raw bytes for a color |
| `array` | Array | An ordered list of values | `[1023, 1044, 1002]` |
| `map` | Map | A named group of fields | `{"x": 1.2, "y": 0.3, "z": 9.8}` |
| `null` | Null | No reading is available right now | `null` |

CBOR chooses the smallest byte encoding for a `uint`, `int`, or `float` value automatically. A
channel's declared type does not state a bit width, such as 8-bit or 32-bit. Where a bit width or
range matters, it is stated in prose next to the channel, such as "0 to 4,294,967,295".

## Enumerated values

An enumerated value is a `text` value, limited to a fixed, named list. The list is stated where
the value is used. Example: `CFG MODE` accepts only the text values `PERIODIC`, `ONCHANGE`, and
`BOTH`. An enumerated value has no separate type name of its own. It is a `text` value with a
stated, limited list of allowed values.

## The "no reading" value

Any channel of any declared type can report the value `null` instead of a normal value. `null`
means the device has no reading for this channel right now. Reasons include: the sensor is not
ready, the sensor failed, or the channel is turned off.

A host reads `null` as "no data", not as zero or false.

## Arrays and maps carry structured data directly

A CBOR `array` holds an ordered list of values, of any length. A CBOR `map` holds a named group
of fields. Both can appear as a channel's value, or nested inside another array or map.

Example: a future 3-axis motion sensor channel, `imu0`, could use the declared type
`map{x: float, y: float, z: float}`, and report the value `{"x": 1.2, "y": 0.3, "z": 9.8}` in
one sample. A map's keys are `text` values.

This is why this project chose CBOR: a future sensor can report a whole named structure as one
value, with no change to the frame design in `docs/03-message-model.md`.

## Flags field

Every sample frame carries a `flags` field. See `docs/03-message-model.md` for its position. The
`flags` field is an unsigned integer, read as a bit field. This table lists every bit defined so
far.

| Bit | Name | Meaning |
|---|---|---|
| 0 | `DROP` | One or more samples were lost before this one. See `docs/08-flow-control-and-backpressure.md`. |
| 1 | `STALE` | This value has not changed since the last time it was read. |
| 2 | `SAT` | The reading hit the minimum or maximum value the sensor can report. |

A `flags` value of `0` means no bit is set. A `flags` value of `1` means only `DROP` is set.

## Channels in this version

| Channel | Declared type | Access | Meaning |
|---|---|---|---|
| `btn0` | `bool` | `RO` | Current state of the push button. `true` means pressed. |
| `btn0_cnt` | `uint` | `RO` | Number of times the button has been pressed since startup. |
| `btn0_us` | `uint` | `RO` | Timestamp, in microseconds since startup, of the last button state change. |
| `led0` | `bytes`, exactly 3 bytes | `RW` | Color of the onboard RGB LED: red, green, blue, in that order. |
| `uptime_us` | `uint` | `RO` | Time in microseconds since the device started. |
| `dist0` | `uint`, or `null` | `RO` | Distance measured by the ultrasonic sensor, in millimeters. `null` if no echo was detected before the ranging timeout — nothing in range, or the sensor is disconnected or faulted. |

### `dist0` range and timing

The sensor wired to this channel reports a valid reading from about 40 mm to 3,000 mm, at a
measuring angle under 15 degrees. See `docs/01-hardware-atoms3-lite.md` and
`docs/KS0504-ultrasonic-sensor-datasheet.pdf`. A `GET dist0` command, or one sample sent while
streaming, triggers one full ranging cycle: this blocks the caller for up to the ranging timeout
defined in `main/ultrasonic.c` (see `docs/12-hardware-abstraction.md`), far longer than any other
channel's read time in this project. `docs/05-command-reference.md` describes how this affects
streaming when `dist0` is subscribed alongside another channel.

`dist0` does not use the `SAT` flags bit for an out-of-range or failed reading. It reports `null`
instead, the same way any channel of any type can.

## Naming a future ADC channel

This section is a plan for a future version, not a channel in this version. A channel that reads
a raw Analog-to-Digital Converter (ADC) measurement will use the suffix `_raw`, with declared type
`uint`, holding the ADC's own count value. A channel that reads the same measurement, converted to
a real-world unit, will use a suffix naming that unit, such as `_mv` for millivolts, with declared
type `int`. This project keeps the raw and converted readings as two separate channels, because
the ESP32-S3 chip's conversion from a raw count to a voltage is not a simple straight-line
formula.
