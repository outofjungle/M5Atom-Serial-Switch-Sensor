# Error Codes

This document lists every error code the device can report. An error code appears as the value
of an `ERR` response, described in `docs/03-message-model.md`. See `docs/00-overview.md` for term
definitions.

## Error code table

| Code | Meaning | What the host should do |
|---|---|---|
| `ECRC` | The received frame's CRC did not match its content. This code is never sent in an `ERR` response. See the note below. It is counted in the `crc_errors` value reported by `STAT`. | Watch the `crc_errors` count with `STAT`. If it rises, check the USB cable and connection. |
| `EFRAME` | The received bytes could not be decoded as a COBS-encoded frame, or the decoded bytes could not be decoded as a CBOR array. | Check the command was built correctly. Resend it. |
| `ECMD` | The verb in the command is not a known verb. | Check the verb spelling against `docs/05-command-reference.md`. |
| `EARG` | An argument is missing, extra, or not a valid value for its position. | Check the argument list against `docs/05-command-reference.md`. |
| `ERANGE` | A value is outside the range this channel or key accepts. | Check the allowed range and resend a value inside it. |
| `ESTATE` | The command does not apply to the device's current state. | Check the device's current state with `STAT`, then retry. |
| `EBUSY` | The device cannot process this command right now. | Wait, then resend the command. |
| `EINTERNAL` | The device found an internal fault while handling the command. | Report this to the firmware developer. Restarting the device with `RST` may help. |
| `EOVF` | An incoming frame was longer than the frame length limit. | Check the frame was built correctly. See `docs/04-cbor-encoding.md`. |
| `ENOCH` | The channel name given is not a known channel. | Check the channel name against a `CAPS` response. |
| `EACCESS` | The command tries to write to a read-only channel, or read a write-only channel. | Check the channel's access mode with `CAPS`. |

## How the device chooses an error code

Only one error code is sent per `ERR` response, even when more than one problem exists in a
command. The device checks in this order and reports the first problem found:

1. Frame check. A failure here sends no response. See the note below.
2. Sequence number check. A failure here also sends no response. See the note below.
3. Command structure check. A failure here reports `EFRAME`.
4. Verb check. A failure here reports `ECMD`.
5. Argument count and type check. A failure here reports `EARG`.
6. Channel or key name check. A failure here reports `ENOCH`.
7. Access mode check. A failure here reports `EACCESS`.
8. Value range check. A failure here reports `ERANGE`.
9. Device state check. A failure here reports `ESTATE`.

A command that passes every check above, but that the device cannot complete for another reason,
reports `EBUSY` or `EINTERNAL`.

## A note on steps 1 and 2

The sequence number is one of the values inside the CRC-protected bytes. If step 1 fails, the
device cannot trust any byte in the frame, including the sequence number, so it cannot send a
response a host could match to the failed command. If step 1 passes but step 2 still cannot find
a well-formed array with a kind and a sequence number in it, the device also has nothing
trustworthy to reply with.

In both cases, the device sends no response. It counts the failure in the `crc_errors` counter
reported by `STAT`, and moves on to the next frame, as described in "Resynchronizing after a bad
frame" in `docs/04-cbor-encoding.md`. Step 2 failing this way is rare: it needs a sender that
built a well-formed CRC over a badly-shaped array, which normal wire corruption does not produce.

Every check from step 3 onward happens after a trustworthy sequence number has been found, so the
device can send a matching `ERR` response for those failures.
