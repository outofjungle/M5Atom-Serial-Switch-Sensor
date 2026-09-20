# Hardware Abstraction

This document states one rule for every channel this project has, or will ever add. See
`docs/00-overview.md` for term definitions.

## The rule

A channel reports clean state. The device decides how that state is produced. The wire protocol
never sees, and never configures, the device's internal signal conditioning.

"Signal conditioning" means any filtering, timing window, or cleanup step needed to turn a raw
electrical reading into a trustworthy value. A mechanical push button needs this: the metal
contacts inside it physically bounce for a short time on every press and release, so a raw read
would report several fast, false transitions instead of one clean one. A different sensor wired
to a future channel might switch cleanly with no bounce at all — a digital output from another
chip, for example — and would need no conditioning.

Only the device knows which case applies, because only the device knows what is physically
attached to each pin. This is why signal conditioning is never a command argument, a `CFG` key,
or anything else the host can read or change. The host only ever asks for a channel's value. It
never asks how that value was cleaned up, because it does not need to know, and a generic wire
protocol cannot assume the answer is the same for every channel.

## Why this was not always true here

An earlier version of this project's `CFG` command included a `DEBOUNCE_US` key, letting a host
set the button's debounce window over serial. This broke the rule above: it exposed a detail of
one specific channel's hardware as if it were a general protocol setting, and it assumed every
future digital input channel would need the same kind of conditioning `btn0` does. `DEBOUNCE_US`
was removed. The button's debounce logic did not change — only where its constant lives moved,
from a value readable and writable over the wire to a private constant inside the one file that
owns the hardware it conditions.

## The two channels in this version

### `btn0` — needs conditioning

`main/sensor_button.c` owns the push button on G41. It debounces the raw GPIO reading with a
fixed 20 millisecond window: an edge is only accepted if at least 20,000 microseconds have passed
since the last accepted edge. This constant is `BTN_DEBOUNCE_US`, defined once, at the top of that
file, and used nowhere else. No command reads or changes it.

### `led0` — needs no conditioning

`main/led.c` owns the WS2812C RGB LED on G35. It is a write-only actuator, not a sensor: a `SET`
command's bytes go straight to the LED driver, with no filtering step of any kind. This is the
simple end of the same rule. A channel that needs no conditioning still follows the rule — its
implementation states that plainly, rather than defending against a case that cannot happen.

## Adding a third channel

State two things, and nothing more:

1. Does this channel's raw signal need conditioning? If yes, name the approach (a debounce window,
   an averaging filter, a minimum-change threshold — whatever the specific hardware needs) and
   keep its constants private to the module that owns that hardware, the way `sensor_button.c`
   does.
2. If it needs none, say so in one sentence, the way this document does for `led0`.

Nothing about the command set, the message model, or the frame encoding changes to add a channel
that follows this rule. That is the reason this rule exists.
