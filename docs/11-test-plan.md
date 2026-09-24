# Test Plan

This document lists the tests used to check that the firmware works correctly. See
`docs/00-overview.md` for term definitions. These tests run after Phase 2 of the project plan
builds the firmware.

## Build and connection tests

| Step | Action | Expected result |
|---|---|---|
| 1 | Run `make build`. | The build finishes with no errors. |
| 2 | Run `make flash`. | The tool reports a successful write, with no errors. |
| 3 | Power on normally, with the button untouched. List the USB serial ports. | Exactly one new port appears. See `docs/09-host-integration.md`. |
| 4 | Check the USB device information on the host computer. | The Vendor ID (VID), Product ID (PID), product name string, and serial number string match `docs/02-usb-device-and-descriptors.md`. |

## Command tests

| Step | Action | Expected result |
|---|---|---|
| 5 | Send `PING`. | Response is `OK`. |
| 6 | Send `ID`. | Response is `OK`, with a name, firmware version, and serial number. |
| 7 | Send `CAPS`. | Response lists every channel in `docs/06-channel-model-and-types.md`, then an `OK` frame with the correct count. |
| 8 | Send `STAT`. | Response is `OK`, with all counters present. |

## Button tests

| Step | Action | Expected result |
|---|---|---|
| 9 | Send `GET btn0` while the button is not pressed. | Response is `OK`, value `0`. |
| 10 | Hold the button down, then send `GET btn0`. | Response is `OK`, value `1`. |
| 11 | Send `GET btn0_cnt`. Press the button once. Send `GET btn0_cnt` again. | The second value is 1 more than the first value. |
| 12 | Press the button 10 times quickly, within 1 second total. | `btn0_cnt` increases by exactly 10. No extra counts appear from electrical bounce. |

## Streaming tests

| Step | Action | Expected result |
|---|---|---|
| 13 | Send `CFG PERIOD_US 100000`. | Response is `OK`. |
| 14 | Send `SUB btn0`. | Response is `OK`. |
| 15 | Send `START`. | Response is `OK`. Sample frames for `btn0` begin arriving about 10 times per second. |
| 16 | While samples are arriving, send `GET uptime_us`. | A normal `OK` response arrives. It is not confused with any `DATA` sample frame. |
| 17 | Send `STOP`. | Response is `OK`. No more sample frames arrive. |

## Failure and recovery tests

| Step | Action | Expected result |
|---|---|---|
| 18 | Send a command frame with an incorrect CRC. | Response is `ERR`, code `ECRC`. |
| 19 | Send a command with a verb not in `docs/05-command-reference.md`. | Response is `ERR`, code `ECMD`. |
| 20 | Send `SET btn0 1`. | Response is `ERR`, code `EACCESS`, because `btn0` is read-only. |
| 21 | Send `GET doesnotexist`. | Response is `ERR`, code `ENOCH`. |
| 22 | Send a frame longer than 256 bytes. | Response is `ERR`, code `EOVF`. The next normal frame sent after this is answered correctly. |
| 22a | Send bytes that do not decode as a valid COBS-encoded frame, followed by a `0x00` byte, followed by a normal `PING` frame. | No response arrives for the malformed bytes. `STAT` shows the `crc_errors` counter went up. The `PING` frame is answered correctly. See the note in `docs/07-error-codes.md` about why no response is sent for this case. |

## Data format tests

| Step | Action | Expected result |
|---|---|---|
| 23 | Start streaming `btn0` and save the raw incoming bytes to a file for 10 seconds. | The file contains a sequence of COBS-encoded, `0x00`-delimited frames. |
| 24 | Decode the saved file using the `iter_frames` function shown in `docs/09-host-integration.md`, and build a table with `pandas`. | Every frame decodes with no CRC failures, and one table row exists per sample sent. |

## Browser access tests

| Step | Action | Expected result |
|---|---|---|
| 27 | Open the page shown in `docs/09-host-integration.md`, in Chrome or Edge. Click "Connect" and choose the device's CDC0 port. | The browser's device picker lists the port. No error appears after choosing it. |
| 28 | With the page connected, check the browser's console. | The `PING` response, `[1, 1, "PING"]`, is printed. |

## Flow control tests

| Step | Action | Expected result |
|---|---|---|
| 25 | Power on with the debug port enabled (see below). Open CDC1 with a terminal program, then close it, while the device is running. | The device keeps running normally. `STAT` may show a nonzero `log_lines_dropped` count. |
| 26 | Start streaming. Close the command port (CDC0) without sending `STOP`. Wait 5 seconds. Reopen CDC0. | `STAT` shows a nonzero `samples_dropped` count. The next sample frame received has the `DROP` bit set in its `flags` field. |

## Debug port tests

| Step | Action | Expected result |
|---|---|---|
| 29 | Power on normally, with the button untouched. | Exactly one port appears. `CFG` has no `DEBOUNCE_US` key: sending `CFG DEBOUNCE_US` returns `ERR`, code `EARG`. |
| 30 | Send `GET btn0_cnt`. Power the device off and on again while holding the button through the entire boot sequence, then send `GET btn0_cnt` again. | Two ports appear this time (`docs/09-host-integration.md`), and log text is visible on the second one. `btn0_cnt` reads the same value as before the restart — holding the button for this gesture does not count as a press. |
| 31 | Press the button 10 times quickly (as in test 12), on firmware built after the debounce change. | `btn0_cnt` still increases by exactly 10. The debounce behavior is unchanged; only who owns its constant moved, from `CFG` to `main/sensor_button.c`. See `docs/12-hardware-abstraction.md`. |

## Ultrasonic sensor tests

| Step | Action | Expected result |
|---|---|---|
| 32 | Hold a flat object about 30 cm from the sensor. Send `GET dist0`. | Response is `OK`, with a value near 300 (millimeters), within the sensor's stated accuracy. |
| 33 | Point the sensor at open air, well past its 3-meter maximum range, or with nothing in front of it. Send `GET dist0`. | Response is `OK`, value `null`. `STAT`'s `ranging_timeouts` counter increases by 1. |
| 34 | Move an object slowly from 5 cm to 250 cm in front of the sensor while sending `GET dist0` repeatedly. | Returned values track the object's approximate distance, increasing as it moves away. |
| 35 | Hold an object closer than 4 cm to the sensor. Send `GET dist0`. | Response is `OK`, with either a small value or `null`; behavior this close is inside the sensor's stated blind zone and is not required to be exact. Record what is observed. |
| 36 | Send `CFG PERIOD_US 50000`, `SUB dist0`, `START` (with no other channel subscribed). | `DATA` frames for `dist0` arrive about 20 times per second. |
| 37 | With `dist0` streaming alone (test 36) still running, also `SUB btn0`, without changing `PERIOD_US`. Press the button while watching `btn0` frames arrive. | `btn0` frames now arrive no faster than about 20 times per second too, not at whatever rate a short `PERIOD_US` alone would otherwise give `btn0`. See `docs/05-command-reference.md`. Send `UNSUB dist0` to confirm `btn0` returns to its normal rate once `dist0` is removed from the subscription list. |
| 38 | Send two `GET dist0` commands back to back, as fast as the host can send them. | Both return a valid response (a value or `null`), with no `ERR`. The second command's response arrives no sooner than the retrigger guard time set in `main/ultrasonic.c` after the first command was issued. |

## Hardware safety check

Do this test before any other test in this document, on first power-on after wiring the sensor.

| Step | Action | Expected result |
|---|---|---|
| 39 | Before first power-on, measure the voltage on G38 (Echo) while the sensor is powered and idle, and during a ranging pulse. | The voltage stays within the ESP32-S3's safe input range at all times. If it does not, stop and fix the voltage divider or level shifter on the Echo line before proceeding with any other test. See `docs/01-hardware-atoms3-lite.md`. |
