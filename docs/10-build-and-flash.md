# Build and Flash

This document describes how the firmware is built and loaded onto the device. See
`docs/00-overview.md` for term definitions.

The build files this document describes are not yet written. This document is written first, as
part of Phase 1 of the project plan, so it can be reviewed before Phase 2 creates the files.

## Build tool

This project uses Espressif IoT Development Framework (ESP-IDF), version 5.4.1, the same version
used by the `Matter-M5NanoC6-Switch` project. The build runs inside a Docker container, so a
developer does not need to install ESP-IDF directly on their computer.

## Why Docker for building, but not for flashing

Docker Desktop for macOS cannot give a container direct access to a USB device. This is a known
limitation of how Docker runs on macOS. For this reason, this project builds the firmware file
inside Docker, then loads that file onto the device using a flashing tool installed directly on
the host computer, outside Docker.

## Planned commands

| Command | What it does |
|---|---|
| `make build` | Builds the firmware inside the Docker container. |
| `make flash` | Loads the built firmware onto the device, using the host computer's `esptool` program. |
| `make monitor` | Opens a text view of the device's log port (CDC1), and saves the output to a log file. |
| `make clean` | Removes build output, inside the Docker container. |
| `make menuconfig` | Opens the ESP-IDF configuration menu, inside the Docker container. |
| `make erase` | Erases the device's flash memory completely. |

## Identifying the two USB ports before flashing

The device presents two ports once its firmware is running. See `docs/09-host-integration.md`
for how to find them. Before the first firmware load, or after `make erase`, the device may not
present any USB CDC-ACM port at all. In that case, follow the download mode steps in
`docs/01-hardware-atoms3-lite.md` first.

## Typical first-time sequence

1. Run `make build` to compile the firmware.
2. Put the device in download mode, following the steps in
   `docs/01-hardware-atoms3-lite.md`, if the device is new or was just erased.
3. Run `make flash` to load the firmware.
4. Run `make monitor` to view log output on CDC1 and confirm the device started correctly.
5. Use a host program, as shown in `docs/09-host-integration.md`, to send commands on CDC0.

## Target chip setting

This project sets `IDF_TARGET` to `esp32s3` in three places, and all three must agree:

| File | Setting |
|---|---|
| `Makefile` | `TARGET := esp32s3` |
| `docker-compose.yml` | `IDF_TARGET=esp32s3` |
| `sdkconfig.defaults` | `CONFIG_IDF_TARGET="esp32s3"` |
