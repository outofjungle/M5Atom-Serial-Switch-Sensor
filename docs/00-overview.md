# Overview

This document explains the goal of this project. It defines the terms that every other document
uses. Read this document first.

## Goal

This project builds firmware for the M5Stack AtomS3 Lite board. The board has an ESP32-S3
microcontroller. The firmware turns the board into a sensor device that connects to a computer by
Universal Serial Bus (USB).

The first version of the device reports the state of one push button. The design must also work
for future boards with more sensors. A later board may add temperature sensors, motion sensors,
or several buttons. The command set in this project must not need a redesign when that happens.
A second version adds exactly such a sensor: an ultrasonic distance sensor, wired to the Grove
port, with no change to the command set. See `docs/01-hardware-atoms3-lite.md` and
`docs/06-channel-model-and-types.md`.

## Scope

This version of the project does these things:

1. It reports the state of the built-in push button.
2. It reports distance, read from an ultrasonic distance sensor wired to the Grove port.
3. It lets the connected computer read the button state or the distance on request.
4. It lets the connected computer turn on automatic reporting at a fixed time interval.
5. It checks every message for transmission errors.

## Out of scope

This version of the project does not do these things:

1. It does not connect to a network. It only uses the USB cable.
2. It does not support more than one sensor board type at once.
3. It does not encrypt the data sent over USB.
4. It does not update its own firmware over USB.

## Terms

This section defines each term. Every other document in this project uses these exact terms.

| Term | Meaning |
|---|---|
| Host | The computer that connects to the device by USB. |
| Device | The M5Stack AtomS3 Lite board running this firmware. |
| Frame | One complete message sent between the host and the device. |
| Command | A frame that the host sends to ask the device to do something. |
| Response | A frame that the device sends to answer one command. |
| Sample | One reading from one channel, sent by the device without a matching command. |
| Channel | One named source of data or one named control on the device. Example: `btn0`. |
| Sequence number | A number in a frame, from 0 to 65535. It links a response to its command. |
| Cyclic Redundancy Check (CRC) | A number computed from a frame. The reader uses it to detect a damaged frame. |
| Verb | The command name inside a frame. Example: `GET`, `PING`, `START`. |
| Concise Binary Object Representation (CBOR) | A binary data format. It can represent numbers, text, byte strings, arrays, and maps, in a compact form. |
| Consistent Overhead Byte Stuffing (COBS) | A method for encoding bytes. It guarantees that one chosen byte value never appears inside the encoded data by accident. This project uses it to mark where one frame ends and the next begins. |
| Streaming | The device sends samples on its own schedule, without a command for each sample. |
| Polling | The host sends a command each time it wants one reading. |

## Document index

| Document | Covers |
|---|---|
| `01-hardware-atoms3-lite.md` | The board: pins, USB wiring, and how to enter firmware download mode. |
| `02-usb-device-and-descriptors.md` | How the device presents itself on the USB bus. |
| `03-message-model.md` | The content of a frame, without regard to how it is written as bytes. |
| `04-cbor-encoding.md` | How a frame is written as bytes, using CBOR, a CRC, and COBS. |
| `05-command-reference.md` | Every command the device accepts, and its response. |
| `06-channel-model-and-types.md` | How channels are named, typed, and listed. |
| `07-error-codes.md` | Every error the device can report, and what it means. |
| `08-flow-control-and-backpressure.md` | What happens when the host stops reading data. |
| `09-host-integration.md` | How to talk to the device from a host program, on each operating system, and from a web browser. |
| `10-build-and-flash.md` | How to build the firmware and load it onto the device. |
| `11-test-plan.md` | The tests used to check that the firmware works correctly. |
| `12-hardware-abstraction.md` | The rule that the device, not the wire protocol, owns hardware-specific signal conditioning. |
| `KS0504-ultrasonic-sensor-datasheet.pdf` | The ultrasonic sensor's own datasheet, from the manufacturer. Not a document this project wrote; kept here for reference. See `docs/01-hardware-atoms3-lite.md`. |

## Design choices and the reasons for them

This section records the main design choices for this project. Each row states the choice and the
reason.

| Area | Choice | Reason |
|---|---|---|
| Frame content | A CBOR array, described in `docs/03-message-model.md` | CBOR carries the type of each value on the wire. A future channel can send a number, text, a list, or a named group of fields, with no change to the surrounding frame design. |
| Frame boundary | COBS, plus a CRC, plus one reserved byte value marking the end of a frame | A binary CBOR frame can contain any byte value. A method is needed to find where one frame ends and the next begins, without confusing a data byte for that marker. COBS solves this with very little added size. See `docs/04-cbor-encoding.md`. |
| Error checking | A CRC on every frame | USB transmission is normally reliable, but the check catches software mistakes and detects a corrupted frame early. |
| USB device type | Two virtual serial ports (see `docs/02-usb-device-and-descriptors.md`) | Splitting the data channel from the log channel keeps debug text out of the data stream. Using the standard serial port class also lets a browser reach the device through the Web Serial API. |
| Browser access | The Web Serial API, not the WebUSB API | The device's two ports use the standard USB serial port class. On every major operating system, that class is claimed by the operating system's own serial driver before a browser tab can reach it directly. The Web Serial API is built to work through that driver. See `docs/09-host-integration.md`. |
| Command design | Verbs work the same way for every channel | A future board can add a channel without adding a new verb. |
| Periodic sampling, with `dist0` added | Kept as an `esp_timer` callback, not moved to a dedicated FreeRTOS task | `dist0`'s read blocks for up to tens of milliseconds, unlike every other channel's near-instant read, which would normally argue for a dedicated task instead of the shared `esp_timer` callback dispatch task. This firmware registers no other periodic `esp_timer` callback -- no Wi-Fi, no BLE -- so there is nothing else for that blocking to delay. See `main/app_stream.c` and `docs/12-hardware-abstraction.md`. |

## Who reviews this

The user reviews every document in `docs/` before any firmware code is written. This is a
requirement for this project, stated at its start.
