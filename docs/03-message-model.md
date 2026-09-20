# Message Model

This document describes the content of a frame. It does not describe how that content becomes
bytes on the wire. See `docs/04-cbor-encoding.md` for that. See `docs/00-overview.md` for term
definitions.

## Frame content is one CBOR array

Every frame's content is a single Concise Binary Object Representation (CBOR) array. CBOR is a
binary data format. It can represent numbers, text, byte strings, arrays, and maps, in a compact
form.

The first item in every frame's array is the frame's kind. The kind is a small unsigned integer.
A reader looks at this first item to know what the rest of the array means.

## Frame kinds

| Kind number | Name | Sent by | Purpose |
|---|---|---|---|
| 0 | `CMD` | Host | Asks the device to do something. |
| 1 | `OK` | Device | Reports that a command succeeded. |
| 2 | `ERR` | Device | Reports that a command failed. |
| 3 | `ROW` | Device | Carries one row of a multi-row answer. |
| 4 | `DATA` | Device | Reports one sample, without a matching command. |

## Command frame

```
[ 0, seq, verb, arg, arg, ... ]
```

| Position | Item | Type | Meaning |
|---|---|---|---|
| 1 | Kind | unsigned integer | Always `0` |
| 2 | Sequence number | unsigned integer | Chosen by the host. Counts from 0 to 65535, then wraps to 0. |
| 3 | Verb | text | The command name, such as `"GET"` or `"PING"` |
| 4 and later | Arguments | any type | Zero or more values. Which values appear depends on the verb. See `docs/05-command-reference.md`. |

## Response frame

```
[ kind, seq, verb, value, value, ... ]
```

| Position | Item | Type | Meaning |
|---|---|---|---|
| 1 | Kind | unsigned integer | `1` (`OK`), `2` (`ERR`), or `3` (`ROW`) |
| 2 | Sequence number | unsigned integer | Copied from the command this response answers |
| 3 | Verb | text | The verb from the command being answered |
| 4 and later | Values | any type | Zero or more result values. An `ERR` frame carries exactly one value: the error code, as text. See `docs/07-error-codes.md`. |

## Multi-row answers

Some commands return more than one row of data. Example: `CAPS` lists every channel on the
device. Each row is sent as its own `ROW` frame, using the same sequence number. A final `OK`
frame, with the same sequence number, marks the end of the answer and states the row count.

Example, showing each frame's array:

1. Command: `[0, 1, "CAPS"]`
2. Response: `[3, 1, "CAPS", "btn0", "bool", "RO"]`
3. Response: `[3, 1, "CAPS", "btn0_cnt", "uint", "RO"]`
4. Response: `[1, 1, "CAPS", 2]` — end of the list, row count `2`

The host reads `ROW` frames until it reads the matching `OK` frame. The host then knows the
answer is complete.

## Sample frame

```
[ 4, seq, t_us, channel, value, flags ]
```

| Position | Item | Type | Meaning |
|---|---|---|---|
| 1 | Kind | unsigned integer | Always `4` |
| 2 | Sequence number | unsigned integer | Chosen by the device. Counts up with each sample sent. Wraps at 65535. |
| 3 | Timestamp | unsigned integer | Microseconds since the device started. |
| 4 | Channel name | text | The channel this sample comes from. |
| 5 | Value | the channel's declared type | The reading. See `docs/06-channel-model-and-types.md`. |
| 6 | Flags | unsigned integer | A bit field. See `docs/06-channel-model-and-types.md` and `docs/08-flow-control-and-backpressure.md`. |

## Why a value needs no separate type field

A value's CBOR bytes already state its own type. A reader does not need an extra field to say
"the next value is a number" or "the next value is text". The reader learns this directly while
it decodes the value.

`docs/06-channel-model-and-types.md` states which type each channel is expected to use. This
statement is a rule for the firmware and the host to follow. It is not sent again inside every
sample frame.

## Sequence numbers

The host picks the sequence number for a command it sends. The device copies that number into
its response. The device picks its own sequence number for each sample frame it sends, and
counts up by 1 each time.

The host uses the sequence number to match a response to the command it sent. This matters when
the host sends a new command before the previous response has arrived.

## Where the frame's content ends and the wire bytes begin

This document describes the array shown above as content, not as bytes. `docs/04-cbor-encoding.md`
describes how this array becomes CBOR bytes, how a CRC is added, and how COBS marks the start and
end of each frame on the wire.
