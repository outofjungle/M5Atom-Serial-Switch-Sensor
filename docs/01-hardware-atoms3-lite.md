# Hardware: M5Stack AtomS3 Lite

This document describes the physical board used in this project. See `docs/00-overview.md` for
term definitions.

## Main chip

| Item | Value |
|---|---|
| Chip | ESP32-S3FN8 |
| CPU | Two Xtensa LX7 cores, 240 MHz |
| Flash memory | 8 megabytes (MB), built into the chip package |
| Extra Random Access Memory (PSRAM) | None |
| USB connector | USB Type-C |

## Pin table

This table lists every pin used or made available by this board.

| Pin | Function | Notes |
|---|---|---|
| G41 | Push button | Pulled high when not pressed. Reads low when pressed. Needs the internal pull-up resistor turned on. |
| G35 | RGB LED data line | Chip type WS2812C-2020. One LED. |
| G4 | Infrared (IR) transmitter | Not used in this version of the firmware. |
| G1 | Grove port, pin 1 | Can act as General Purpose Input/Output (GPIO), Inter-Integrated Circuit (I2C), or Universal Asynchronous Receiver/Transmitter (UART). |
| G2 | Grove port, pin 2 | Same options as G1. |
| G5 | Header pin | Also usable as an Analog-to-Digital Converter (ADC) input, ADC unit 1. |
| G6 | Header pin | Also usable as ADC input, ADC unit 1. |
| G7 | Header pin | Also usable as ADC input, ADC unit 1. |
| G8 | Header pin | Also usable as ADC input, ADC unit 1. |
| G38 | Header pin | General purpose. |
| G39 | Header pin | General purpose. |
| G43 | UART0 transmit | Not connected to any external header on this board. |
| G44 | UART0 receive | Not connected to any external header on this board. |

## ADC-capable pins

Pins G1, G2, G5, G6, G7, and G8 connect to ADC unit 1 on the ESP32-S3 chip. A future sensor board
can read an analog voltage on any of these pins. This project's firmware does not use the ADC in
its first version. `docs/06-channel-model-and-types.md` describes how an ADC channel will be
named and typed when one is added.

## USB wiring

The USB Type-C connector wires directly to the USB-OTG (On-The-Go) peripheral inside the
ESP32-S3 chip. There is no separate USB-to-serial bridge chip on this board.

This wiring choice has two results:

1. The device can present a custom USB Vendor ID (VID) and Product ID (PID). See
   `docs/02-usb-device-and-descriptors.md`.
2. The USB-OTG peripheral runs at USB Full Speed only. The ESP32-S3 chip does not support USB
   High Speed. This limits each USB data packet to 64 bytes.

## UART0 is not available

The ESP32-S3 chip has a built-in serial port called UART0, on pins G43 and G44. This board does
not connect those pins to any external header or connector. This means UART0 cannot be used to
view log messages on this board. `docs/02-usb-device-and-descriptors.md` explains how this
project sends log messages instead.

## Two different button gestures at power-up

The button on G41 controls two unrelated things, depending on when it is held. Read this section
before assuming one gesture does what the other does.

| Gesture | What it does | When it is checked |
|---|---|---|
| Hold for about 2 seconds, until the green LED lights, then release | Enters the ROM's firmware download mode | By the chip itself, before any of this project's code runs |
| Hold through the entire boot sequence, from power-on until the device finishes starting | Turns on the debug (log) port, CDC1 | By this project's firmware, early in `app_main()` |

These do not combine or interact. The first happens at the hardware level and this project's code
has no part in it. The second is this project's own firmware checking the button once, right
after it is safe to read, and is described next.

## Entering firmware download mode

The board needs to be in download mode before a new firmware file can be loaded onto it.
Automatic download mode only works while the device is running M5Stack's own USB-Serial-JTAG
based bootloader behavior. Once this project's own firmware has run at least once, it takes over
the same USB hardware for its own CDC-ACM port or ports, and automatic download mode no longer
works. Use this manual procedure instead:

1. Make sure the USB cable is already plugged in.
2. Press and hold the side button (the button on G41).
3. Keep holding until the green LED lights up. This takes about 2 seconds.
4. Release the button. The green LED turns off.
5. The board is now in download mode. Run the flash command described in
   `docs/10-build-and-flash.md`.

## Enabling the debug (log) port

By default, this project's firmware presents only one USB serial port: CDC0, the command and
data port described in `docs/02-usb-device-and-descriptors.md`. The debug port, CDC1, which
carries plain-text log output, does not exist unless it is turned on.

To turn it on, hold the button on G41 down while plugging in the USB cable, or while the device
otherwise powers on, and keep holding until the device has finished starting (a couple of
seconds). Holding the button this way does not count as a button press: `btn0_cnt` is unaffected,
because the firmware only checks the button's raw level once, at boot, before it starts counting
edges.

This exists so that a host connecting to the device normally sees exactly one port, with nothing
to choose between. See `docs/02-usb-device-and-descriptors.md` for what changes in the USB
descriptor when the debug port is enabled, and `docs/09-host-integration.md` for how to find
either port from a host program.
