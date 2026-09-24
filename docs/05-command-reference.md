# Command Reference

This document lists every command the device accepts. See `docs/03-message-model.md` for the
frame content and `docs/04-cbor-encoding.md` for the byte format. See `docs/00-overview.md` for
term definitions.

Each command section below shows the arguments the host sends, and the values the device sends
back. Values are shown by name and type. Each type shown is a CBOR type, carried directly in the
frame's array. See `docs/06-channel-model-and-types.md` for the full type list.

## System commands

### PING

Checks that the device is present and responding.

| Direction | Verb | Arguments |
|---|---|---|
| Command | `PING` | None |
| Response, success | `OK` | None |

### ID

Reports fixed information about the device.

| Direction | Verb | Arguments |
|---|---|---|
| Command | `ID` | None |
| Response, success | `OK` | `name` (text), `fw_version` (text), `serial` (text) |

`serial` is the same value used as the USB serial number string. See
`docs/02-usb-device-and-descriptors.md`.

### CAPS

Lists every channel the device supports. This is a multi-row command. See "Multi-row answers" in
`docs/03-message-model.md`.

| Direction | Verb | Arguments |
|---|---|---|
| Command | `CAPS` | None |
| Response, one per channel | `ROW` | `channel` (text), `type` (text, one of the type names in `docs/06-channel-model-and-types.md`), `access` (text: `RO`, `WO`, `RW`) |
| Response, end of list | `OK` | `count` (unsigned integer) |

### STAT

Reports counters about the device's operation, for diagnosis.

| Direction | Verb | Arguments |
|---|---|---|
| Command | `STAT` | None |
| Response, success | `OK` | `uptime_us` (unsigned integer), `frames_rx` (unsigned integer), `frames_tx` (unsigned integer), `crc_errors` (unsigned integer), `samples_dropped` (unsigned integer), `log_lines_dropped` (unsigned integer), `ranging_timeouts` (unsigned integer) |

`ranging_timeouts` counts every ranging cycle that finished with no echo detected before the
timeout in `main/ultrasonic.c`. See `docs/12-hardware-abstraction.md`.

### RST

Restarts the device. The device sends its response before it restarts.

| Direction | Verb | Arguments |
|---|---|---|
| Command | `RST` | None |
| Response, success | `OK` | None |

### ECHO

Sends back the text the host provides. Used to test that a text value survives the round trip
correctly.

| Direction | Verb | Arguments |
|---|---|---|
| Command | `ECHO` | `text` (text) |
| Response, success | `OK` | `text` (text), the same value sent back |

## Data commands

### GET

Reads the current value of one channel, or every channel. For `dist0`, this triggers one complete
ranging cycle and blocks until it completes or times out; see
`docs/06-channel-model-and-types.md`.

| Direction | Verb | Arguments |
|---|---|---|
| Command, one channel | `GET` | `channel` (text) |
| Command, all channels | `GET` | `"*"` |
| Response, one channel | `OK` | `value` (the channel's declared type, or `null`) |
| Response, all channels, one per channel | `ROW` | `channel` (text), `value` (the channel's declared type, or `null`) |
| Response, all channels, end of list | `OK` | `count` (unsigned integer) |
| Response, unknown channel | `ERR` | `ENOCH` |

Example: `GET dist0` while an object sits 30 cm from the sensor returns `[1, seq, "GET", 300]` — a
value in millimeters. `GET dist0` with nothing in range returns `[1, seq, "GET", null]`.

### SET

Writes a value to one channel. Only a channel marked `WO` or `RW` in `CAPS` accepts this
command.

| Direction | Verb | Arguments |
|---|---|---|
| Command | `SET` | `channel` (text), `value` (the channel's declared type) |
| Response, success | `OK` | None |
| Response, channel is read-only | `ERR` | `EACCESS` |
| Response, unknown channel | `ERR` | `ENOCH` |
| Response, value out of range | `ERR` | `ERANGE` |

## Streaming commands

Streaming sends `DATA` sample frames on a fixed schedule, for one or more subscribed channels.
See `docs/00-overview.md` for the definition of streaming.

### CFG

Reads or writes one streaming configuration key.

| Direction | Verb | Arguments |
|---|---|---|
| Command, write | `CFG` | `key` (text), `value` |
| Command, read | `CFG` | `key` (text) |
| Response, success | `OK` | `value` (the key's type) |
| Response, unknown key | `ERR` | `EARG` |
| Response, value out of range | `ERR` | `ERANGE` |

Configuration keys:

| Key | Type | Meaning | Allowed values |
|---|---|---|---|
| `PERIOD_US` | unsigned integer | Time between samples, in microseconds | 1,000 to 3,600,000,000 (1 hour) |
| `MODE` | text | When a sample is sent | `PERIODIC`, `ONCHANGE`, `BOTH` |
| `BATCH` | unsigned integer | Number of samples grouped into one frame | 1 to 32 |
| `ENCODING` | text | The byte format used for all frames | `CBOR` |

There is no key to configure how a channel cleans up its own raw signal, such as a debounce
window, or how often `dist0` can physically produce a new reading. That is a device-internal
decision, never a wire setting. See `docs/12-hardware-abstraction.md`.

`PERIOD_US`'s minimum, 1,000, was chosen for `btn0`. It is not raised for `dist0`, because
`dist0`'s own physical timing already self-limits: one ranging cycle needs at least 50
milliseconds between triggers (see `docs/12-hardware-abstraction.md`). Setting a shorter
`PERIOD_US` does not error; the device just cannot honor it while `dist0` is subscribed.

**Subscribing `dist0` alongside another channel slows both down together.** Every subscribed
channel is sampled from the same periodic callback, once per `PERIOD_US` tick, in one pass; see
`main/app_stream.c`. `dist0`'s read blocks that pass for up to its own minimum cycle time. This
means `btn0`, if subscribed at the same time as `dist0`, is also only sampled about as often as
`dist0` allows — not at `btn0`'s own faster native rate. Subscribe `dist0` on its own streaming
session, at a period of 50,000 or more, to get `dist0` samples at a predictable rate without this
effect; keep `dist0` unsubscribed on a session that needs `btn0` at a fast, undelayed rate.

`ENCODING` accepts only `CBOR` in this version. This key exists so a future, different byte
format can be selected later, without changing the command that selects it. See
`docs/03-message-model.md`.

### SUB

Adds one channel to the list of channels sent while streaming is active.

| Direction | Verb | Arguments |
|---|---|---|
| Command | `SUB` | `channel` (text) |
| Response, success | `OK` | None |
| Response, unknown channel | `ERR` | `ENOCH` |

### UNSUB

Removes one channel from the streaming list.

| Direction | Verb | Arguments |
|---|---|---|
| Command | `UNSUB` | `channel` (text) |
| Response, success | `OK` | None |
| Response, channel was not subscribed | `ERR` | `ESTATE` |

### START

Begins streaming. The device sends `DATA` sample frames for every subscribed channel, at the
interval set by `PERIOD_US`, until the host sends `STOP`.

| Direction | Verb | Arguments |
|---|---|---|
| Command | `START` | None |
| Response, success | `OK` | None |
| Response, already streaming | `ERR` | `ESTATE` |
| Response, no channel subscribed | `ERR` | `ESTATE` |

### STOP

Ends streaming.

| Direction | Verb | Arguments |
|---|---|---|
| Command | `STOP` | None |
| Response, success | `OK` | None |
| Response, not streaming | `ERR` | `ESTATE` |

## Storage commands

### SAVE

Writes the current streaming configuration and subscription list to Non-Volatile Storage (NVS).
The device reloads these values automatically after a restart.

| Direction | Verb | Arguments |
|---|---|---|
| Command | `SAVE` | None |
| Response, success | `OK` | None |

### LOAD

Reads the streaming configuration and subscription list from NVS, replacing the current values.

| Direction | Verb | Arguments |
|---|---|---|
| Command | `LOAD` | None |
| Response, success | `OK` | None |
| Response, no saved data | `ERR` | `ESTATE` |

## Unknown verb

If the device receives a command with a verb not listed in this document, it responds:

| Direction | Verb | Arguments |
|---|---|---|
| Response | `ERR` | `ECMD` |

The response repeats the verb field exactly as it was received, so the host can identify which
command failed.
