# Host Integration

This document describes how a host program connects to the device, and which libraries handle
each part of the protocol. See `docs/00-overview.md` for term definitions.

## Libraries used on each platform

| Layer | Microcontroller (C) | Host program (Python) | Web browser (JavaScript) |
|---|---|---|---|
| Concise Binary Object Representation (CBOR) | TinyCBOR | `cbor2` (from PyPI) | `cbor-x` (from npm) |
| Consistent Overhead Byte Stuffing (COBS) | A small, vendored reference implementation. About 30 lines. No dependency needed. | `cobs` (from PyPI) | A small, vendored reference implementation. About 30 lines. No dependency needed. |
| Cyclic Redundancy Check (CRC) | A small, vendored implementation. About 12 lines. No dependency needed. | `binascii.crc_hqx`, already in the Python standard library | A small, vendored implementation. About 12 lines. No dependency needed. |
| Serial transport | `esp_tinyusb`, an ESP-IDF managed component | `pyserial` (from PyPI) | The Web Serial API, built into the browser. No library needed. |

CBOR and CRC libraries are widely used and well tested on every platform in this table. COBS is a
small enough algorithm that most projects, including this one, include a short, direct copy of a
reference implementation rather than add a dependency for it, except on the host, where the
`cobs` package on PyPI is a small, stable, and convenient choice.

## Finding the port, or ports

By default, the device presents one CDC-ACM port: CDC0, for commands and samples. A second port,
CDC1, for log text, only exists if it was enabled at boot by holding the button on G41 through
power-up. See `docs/01-hardware-atoms3-lite.md` and `docs/02-usb-device-and-descriptors.md`. A
host program written against this project should normally expect exactly one port.

When the debug port is enabled, both appear, and the operating system creates them in the same
order as the USB interface numbers — CDC0 always gets the lower-numbered or earlier-listed port:

| Operating system | CDC0 (commands and samples) | CDC1 (log text) |
|---|---|---|
| macOS | `/dev/cu.usbmodem<ID>1` | `/dev/cu.usbmodem<ID>3` |
| Linux | `/dev/ttyACM0` | `/dev/ttyACM1` |
| Windows 10 and later | `COM<N>` (lower number) | `COM<N+1>` (higher number) |

`<ID>` on macOS is a number based on the physical USB port used. It can change if the device is
plugged into a different port on the computer.

A host's port picker cannot tell the two ports apart by name — both show the device's product
string. If a host program cannot otherwise be sure which port it opened, sending `PING` and
checking for a reply is the reliable way to confirm it: only CDC0 answers.

## macOS: use the `cu` device, not the `tty` device

macOS creates two device file names for each serial port: one starting with `cu.` and one
starting with `tty.`. A host program must open the `cu.` device. The `tty.` device waits for a
carrier detect signal before it finishes opening. This device does not send that signal, so
opening the `tty.` device blocks and never returns.

## Linux: stop ModemManager from probing the device

Linux's ModemManager service sends AT command text to a newly connected CDC-ACM device, to check
if it is a modem. This device is not a modem, and this text arriving on CDC0 while a host program
is reading would look like corrupted input.

Add a rule file, for example `/etc/udev/rules.d/99-m5atom-sensor.rules`, with this content:

```
SUBSYSTEM=="tty", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="XXXX", ENV{ID_MM_DEVICE_IGNORE}="1"
```

Replace `XXXX` with the actual Product ID (PID) once it is assigned. See
`docs/02-usb-device-and-descriptors.md`.

## Windows

Windows 10 and later versions include a built-in driver, `usbser.sys`, for the CDC-ACM class.
No extra driver file is needed. The two ports appear as two separate `COM` port numbers.

## Reference client, in Python

This section shows a minimal Python program. It builds a `PING` command frame, following the
three steps in `docs/04-cbor-encoding.md`, sends it, and reads the response.

```python
import cbor2
import serial
from binascii import crc_hqx
from cobs import cobs

PORT = "/dev/cu.usbmodem14201"  # replace with the actual CDC0 port
BAUD = 115200  # ignored by a USB CDC-ACM device, but pyserial requires a value

def build_frame(items: list) -> bytes:
    payload = cbor2.dumps(items)
    crc = crc_hqx(payload, 0xFFFF)
    framed = payload + crc.to_bytes(2, "big")
    return cobs.encode(framed) + b"\x00"

def read_frame(port: serial.Serial) -> list:
    raw = port.read_until(b"\x00")
    encoded = raw[:-1]  # drop the trailing delimiter
    framed = cobs.decode(encoded)
    payload, received_crc = framed[:-2], int.from_bytes(framed[-2:], "big")
    if crc_hqx(payload, 0xFFFF) != received_crc:
        raise ValueError("CRC check failed")
    return cbor2.loads(payload)

with serial.Serial(PORT, BAUD, timeout=1) as port:
    port.write(build_frame([0, 1, "PING"]))  # kind=CMD, seq=1, verb="PING"
    print(read_frame(port))  # expect [1, 1, "PING"]  (kind=OK, seq=1, verb="PING")
```

The baud rate value is ignored by a USB CDC-ACM device, because there is no real serial line
with a fixed speed. `pyserial` still requires a value to be given.

## Reference client, in a web browser, using the Web Serial API

The Web Serial API lets a web page open the device's CDC0 port directly, without a separate host
program. It is supported in Chrome and Edge. It cannot open a port on its own; a person must
click a button, and the browser asks them to choose the device.

This example builds the same `PING` frame as the Python example above, and prints the response
to the browser's console.

```html
<button id="connect">Connect</button>
<script type="module">
import { encode, decode } from "https://esm.sh/cbor-x@1";

// A short, direct copy of the COBS algorithm. See docs/04-cbor-encoding.md.
function cobsEncode(data) {
  const out = [];
  let idx = 0;
  while (true) {
    const zeroIdx = data.indexOf(0, idx);
    const end = zeroIdx === -1 ? data.length : zeroIdx;
    const chunk = data.slice(idx, end);
    out.push(chunk.length + 1, ...chunk);
    if (zeroIdx === -1) break;
    idx = zeroIdx + 1;
  }
  return new Uint8Array(out);
}

function cobsDecode(data) {
  const out = [];
  let idx = 0;
  while (idx < data.length) {
    const code = data[idx];
    out.push(...data.slice(idx + 1, idx + code));
    idx += code;
    if (code < 0xff && idx < data.length) out.push(0);
  }
  return new Uint8Array(out);
}

async function crc16(bytes) {
  // CRC-16/CCITT-FALSE, poly 0x1021, init 0xFFFF. See docs/04-cbor-encoding.md.
  let crc = 0xffff;
  for (const b of bytes) {
    crc ^= b << 8;
    for (let i = 0; i < 8; i++) {
      crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff;
    }
  }
  return crc;
}

document.getElementById("connect").addEventListener("click", async () => {
  const port = await navigator.serial.requestPort();
  await port.open({ baudRate: 115200 }); // baudRate is ignored, but required

  const payload = encode([0, 1, "PING"]); // kind=CMD, seq=1, verb="PING"
  const crc = await crc16(payload);
  const framed = new Uint8Array([...payload, crc >> 8, crc & 0xff]);
  const frame = new Uint8Array([...cobsEncode(framed), 0x00]);

  const writer = port.writable.getWriter();
  await writer.write(frame);
  writer.releaseLock();

  const reader = port.readable.getReader();
  let bytes = [];
  while (true) {
    const { value, done } = await reader.read();
    if (done) break;
    bytes.push(...value);
    const zeroIdx = bytes.indexOf(0);
    if (zeroIdx !== -1) {
      const encoded = new Uint8Array(bytes.slice(0, zeroIdx));
      const decoded = cobsDecode(encoded);
      const responsePayload = decoded.slice(0, -2);
      console.log(decode(responsePayload)); // expect [1, 1, "PING"]
      break;
    }
  }
  reader.releaseLock();
});
</script>
```

This example skips the CRC check on the received frame, to keep the example short. A finished
implementation checks it the same way the Python example does.

## Web Serial, not WebUSB

The device's two ports use the standard CDC-ACM class. On every major operating system, that
class is claimed by the operating system's own serial driver. The WebUSB API can only open an
interface that no driver has already claimed, so it cannot reach this device. The Web Serial API
is built to work through the operating system's serial driver, so it is the correct choice for
this device.

## Reading a captured stream as a table

A captured stream of frames is binary, so it must be split and decoded before it becomes a table.
This example reads a file of concatenated wire frames, decodes each `DATA` sample frame, and
builds a table with the `pandas` library.

```python
import cbor2
import pandas as pd
from binascii import crc_hqx
from cobs import cobs

def iter_frames(data: bytes):
    start = 0
    while True:
        end = data.find(b"\x00", start)
        if end == -1:
            break
        framed = cobs.decode(data[start:end])
        payload, received_crc = framed[:-2], int.from_bytes(framed[-2:], "big")
        if crc_hqx(payload, 0xFFFF) == received_crc:
            yield cbor2.loads(payload)
        start = end + 1

rows = []
with open("capture.bin", "rb") as f:
    for item in iter_frames(f.read()):
        if item[0] == 4:  # kind == DATA
            _, seq, t_us, channel, value, flags = item
            rows.append({"seq": seq, "t_us": t_us, "channel": channel, "value": value, "flags": flags})

df = pd.DataFrame(rows)
```
