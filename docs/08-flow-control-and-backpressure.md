# Flow Control and Backpressure

This document describes what the device does when the host is not reading data fast enough, or
not reading it at all. See `docs/00-overview.md` for term definitions.

## The problem

A USB CDC-ACM port has a chain of buffers between the firmware and the host program:

1. A buffer inside the firmware's USB software.
2. A buffer inside the USB hardware.
3. A buffer inside the host operating system's driver.

Each buffer has a fixed size. If the host program stops reading, every buffer in this chain
fills up, starting from the last one and working back.

On Linux, the `cdc_acm` driver only requests data from the device while the host program has the
serial port open. If the host program closes the port, the driver stops requesting data
completely. A host program that keeps the port open but reads slowly causes the same result,
just more slowly.

## Rule 1: a logging call must never wait for USB

The function `tinyusb_cdcacm_write_flush()` can wait for space to become available in the USB
buffer. `ESP_LOG`, the firmware's logging function, can be called from many different tasks, and
from interrupt handlers. `ESP_LOG` also holds an internal lock while it runs.

If a logging call waits for USB, and USB is not being read, that wait can last a very long time.
Because of the internal lock, this wait would also stop every other task that tries to log a
message. This firmware must not let this happen.

## Rule 2: log text goes through a buffer, not directly to USB

The firmware sets a custom output function for `ESP_LOG`, using `esp_log_set_vprintf()`. This
custom function does two things:

1. It formats the log line into a fixed-size ring buffer, in memory.
2. It returns immediately. It never waits for USB.

If the ring buffer is full, the custom function drops the new log line. It does not wait for
space. The firmware counts each dropped line. `STAT`, described in `docs/05-command-reference.md`,
reports this count as `log_lines_dropped`.

## Rule 3: one task writes the log buffer to USB

A separate FreeRTOS task, running at low priority, reads the ring buffer described in Rule 2 and
writes its content to CDC1. This task is the only part of the firmware allowed to wait for CDC1
to have free space.

## Rule 4: check the connection before writing

Before writing to CDC1, this task checks whether a host program is connected, using the function
`tud_cdc_n_connected()`. If no host program is connected, the task discards the log line at once,
instead of trying to write it.

## Rule 5: a lost sample must be reported, not hidden

CDC0 carries samples, described in `docs/03-message-model.md`. A sample is data, not a log
message. The firmware must not silently drop a sample the way it drops a log line.

When CDC0's output buffer is full and a new sample cannot be sent, the firmware does this:

1. It drops the sample that does not fit.
2. It adds 1 to the `samples_dropped` counter, reported by `STAT`.
3. It sets the `DROP` bit in the `flags` field of the next sample frame it does send.

This tells the host, as soon as the connection recovers, that at least one sample was lost. The
host does not have to guess from a gap in timestamps.

## Rule 6: DTR can stop streaming, but does not start it

The Data Terminal Ready (DTR) signal is part of the CDC-ACM standard. Many host programs set DTR
when they open a serial port. The firmware may use a DTR signal change on CDC0 to automatically
send a `STOP` command to itself, ending streaming when the host program closes the port.

The firmware does not require DTR to be set before it will start streaming, because some host
programs and libraries do not set this signal. `START`, described in `docs/05-command-reference.md`,
depends only on the `START` command being received, never on the DTR signal.

## Rule 7: buffer sizes

| Port | Setting | Value |
|---|---|---|
| CDC0 | `CONFIG_TINYUSB_CDC_RX_BUFSIZE` | 512 bytes |
| CDC0 | `CONFIG_TINYUSB_CDC_TX_BUFSIZE` | 1024 bytes |
| CDC1 | `CONFIG_TINYUSB_CDC_RX_BUFSIZE` | Default size |
| CDC1 | `CONFIG_TINYUSB_CDC_TX_BUFSIZE` | Default size |

CDC1 keeps the default, smaller buffer size, because a dropped log line is an acceptable outcome
by design. CDC0 uses a larger buffer to reduce how often Rule 5 applies during normal use.

## How fast a buffer fills

These numbers help explain why Rule 5 matters, using the 1024-byte CDC0 transmit buffer size
from Rule 7.

| Case | Frame size | Sample rate | Time to fill the buffer |
|---|---|---|---|
| Button channel, `PERIOD_US` set to 100,000 (10 samples per second) | 20 bytes, see the worked example in `docs/04-cbor-encoding.md` | 10 per second | About 5 seconds |
| A future Analog-to-Digital Converter (ADC) channel at 1,000 samples per second | About 20 bytes | 1,000 per second | About 51 milliseconds |

These numbers show that a host program must read CDC0 continuously while streaming is active,
especially for a high-rate channel added in a future version.
