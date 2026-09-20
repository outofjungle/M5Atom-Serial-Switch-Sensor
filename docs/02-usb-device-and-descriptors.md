# USB Device and Descriptors

This document describes how the device presents itself on the USB bus. See
`docs/00-overview.md` for term definitions.

## USB device class

The device uses the USB Communications Device Class (CDC), Abstract Control Model (ACM)
subclass. This is the standard USB class for a virtual serial port. A host operating system
already has a driver for this class. No custom driver is needed.

The device presents one CDC-ACM port by default, and a second one if the debug port is turned on:

| Port | Purpose | Present by default? |
|---|---|---|
| CDC0 | Carries commands, responses, and samples. See `docs/03-message-model.md`. | Yes, always |
| CDC1 | Carries log text for a person to read. Carries no commands or samples. | No — only when enabled. See `docs/01-hardware-atoms3-lite.md`. |

Using this standard class also lets a web browser reach CDC0 through the Web Serial API. See
`docs/09-host-integration.md`.

## Why a second port, and why it is off by default

The ESP32-S3 chip has one USB PHY (Physical Layer) circuit. This circuit can run the USB-OTG
peripheral or the USB-Serial-JTAG peripheral, but not both at the same time. This board wires
its USB connector to the USB-OTG peripheral. See `docs/01-hardware-atoms3-lite.md`.

This means the built-in USB-Serial-JTAG log output is not available. When log output is wanted,
the firmware must carry it over the same USB-OTG peripheral as commands, on a second CDC-ACM
port, to keep log text out of the command and response stream.
`docs/08-flow-control-and-backpressure.md` describes how the firmware manages each port.

A host's port picker cannot tell CDC0 and CDC1 apart by name (see "String descriptors" below), so
a device that always presented both would always show a person two identical-looking choices.
Making the second port exist only when deliberately turned on removes that choice for the normal
case: with the debug port off, there is only one port, and nothing to pick between.

## Device descriptor

A device descriptor is the first block of information a USB device sends to a host. This table
lists the values this device sends.

| Field | Value | Meaning |
|---|---|---|
| bDeviceClass | 0xEF | Miscellaneous class. This value is required when a device has more than one CDC-ACM port. |
| bDeviceSubClass | 0x02 | Common Class |
| bDeviceProtocol | 0x01 | Interface Association Descriptor (IAD) |
| bMaxPacketSize0 | 64 | Maximum bytes in one control transfer packet |
| idVendor | 0x303A | Espressif Systems |
| idProduct | Not yet assigned | See "Vendor ID and Product ID" below |
| bcdDevice | 0x0100 | Firmware version 1.00 |
| iManufacturer | String index 1 | See "String descriptors" below |
| iProduct | String index 2 | See "String descriptors" below |
| iSerialNumber | String index 3 | See "Serial number" below |
| bNumConfigurations | 1 | The device offers one configuration |

## Vendor ID and Product ID

A Vendor ID (VID) identifies the company that made a USB device. A Product ID (PID) identifies
the specific product, within that company's VID.

This device uses VID `0x303A`. Espressif Systems, the maker of the ESP32-S3 chip, owns this VID.
Espressif allows any product built on their chip to use a PID under this VID, at no cost.

The PID is requested through a pull request to the public list at
`github.com/espressif/usb-pids`. A specific PID number for this project is an open item until
that request is filed and approved.

Two other options exist and are recorded here for reference:

1. VID `0x1209` is a free, shared VID for open-source hardware projects, managed at `pid.codes`.
2. Choosing a VID or PID number without registering it can cause the host operating system to
   load the wrong driver, or to apply a rule meant for a different device. This project does not
   do this.

## String descriptors

| String index | Content |
|---|---|
| 1 (Manufacturer) | "M5Atom Project" |
| 2 (Product) | "Switch Sensor" |
| 3 (Serial number) | Built from the chip's built-in Media Access Control (MAC) address |
| 4 (CDC0 interface name) | "Switch Sensor Data" |
| 5 (CDC1 interface name) | "Switch Sensor Debug" |

Indices 4 and 5 name each CDC-ACM interface individually, using the interface string field
(iInterface) that the USB standard provides for exactly this purpose. This is the correct,
standard way to give two interfaces of one device two different names.

It does not solve the port-picker problem it was meant to solve. A host's port picker — tested on
Chrome, on macOS — shows the same product name (index 2) for both ports and does not read the
interface string at all. The two ports remain visually identical there except for the port path
number the operating system appends, described in `docs/09-host-integration.md`. This is a
limitation of that picker, not something a string value can fix. The indices are kept anyway,
because other tools (Linux's `udevadm info`, for one) do read and display them.

## Serial number

The serial number string comes from the ESP32-S3 chip's factory-programmed MAC address. The
firmware reads this address and writes it as 12 hexadecimal characters. Example:
`A1B2C3D4E5F6`.

This serial number lets a host tell two devices apart when more than one is plugged in at the
same time.

## Configuration and interfaces

The device offers one USB configuration, built as one of two complete, independently valid
descriptor tables, chosen once at boot depending on whether the debug port is enabled. These are
two different tables, not one table with part of it left out.

With the debug port off (the default), the configuration contains two USB interfaces, one
function:

```
Configuration 1 (debug port off)
  IAD, interfaces 0 and 1: CDC0, "Switch Sensor Data"
    Interface 0: CDC Control
      Endpoint 0x81, Interrupt IN, 8 bytes   (required by the CDC standard, not used)
    Interface 1: CDC Data
      Endpoint 0x02, Bulk OUT, 64 bytes      (commands, from host to device)
      Endpoint 0x82, Bulk IN, 64 bytes       (responses and samples, device to host)
```

With the debug port on, a second function is added, contained in four USB interfaces total,
grouped into two functions by two Interface Association Descriptors (IADs):

```
Configuration 1 (debug port on)
  IAD, interfaces 0 and 1: CDC0, "Switch Sensor Data"
    Interface 0: CDC Control
      Endpoint 0x81, Interrupt IN, 8 bytes   (required by the CDC standard, not used)
    Interface 1: CDC Data
      Endpoint 0x02, Bulk OUT, 64 bytes      (commands, from host to device)
      Endpoint 0x82, Bulk IN, 64 bytes       (responses and samples, device to host)

  IAD, interfaces 2 and 3: CDC1, "Switch Sensor Debug"
    Interface 2: CDC Control
      Endpoint 0x83, Interrupt IN, 8 bytes   (required by the CDC standard, not used)
    Interface 3: CDC Data
      Endpoint 0x04, Bulk OUT, 64 bytes      (from host to device, ignored)
      Endpoint 0x84, Bulk IN, 64 bytes       (log text, device to host)
```

## Endpoint count

With the debug port off, the device uses 2 interfaces and 3 endpoints, plus endpoint 0. With it
on, that grows to 4 interfaces and 6 endpoints, plus endpoint 0 — 4 IN endpoints and 2 OUT
endpoints, counting endpoint 0 as both.

Each CDC-ACM function must include one Interrupt IN endpoint, even when the firmware never sends
data on it. With the debug port enabled, this design has two such endpoints. Adding a third
CDC-ACM function later would need careful planning of the chip's shared transmit buffer space.

## Packet size limit

The USB-OTG peripheral on the ESP32-S3 chip runs at USB Full Speed. USB Full Speed limits a bulk
data packet to 64 bytes. Any frame under 64 bytes fits in a single USB packet. A longer frame is
split into more than one packet automatically by the USB hardware and driver. The firmware does
not need special code for this split.

## Power

| Field | Value |
|---|---|
| bmAttributes | 0xA0 (bus-powered, remote wakeup) |
| bMaxPower | 50 (meaning 100 milliamps (mA)) |

This is a conservative default from the descriptor-building macro this project uses. It has not
been tuned against this board's actual current draw, and can be raised if a future peripheral
needs more.
