# Lamp serial protocol

The firmware emits binary snapshots at **115200 baud, 8N1**, with no flow control
and accepts bounded program-selection and capture commands. It attempts one LED packet after every four rendered LED frames
(about 30 Hz). Only `Lampe::update()` writes to serial. It checks that the entire
packet fits in the AVR UART transmit queue before encoding/writing; a busy queue
causes that snapshot to be dropped. There is no retry, acknowledgement, connection
allocation, or dependence on browser activity for rendering. Optional microphone
capture uses a renewable host request, described below.

Each version 4 packet is exactly 63 bytes. Its layout matches version 2; version 3
adds host program commands and version 4 adds optional microphone capture. The dashboard also accepts version 3, version 2
and the older 61-byte version 1 format (without tempo fields). Both older versions
remain view-only; program buttons are disabled with an update-firmware tooltip.

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 2 | Magic bytes `0x4c 0x4d` (`LM`) |
| 2 | 1 | Version: `4` (program commands and microphone capture supported) |
| 3 | 1 | LED count: `16` |
| 4 | 1 | Program ID, indexed from zero in `ProgramList.def` |
| 5 | 1 | Global brightness, 0–255 |
| 6 | 1 | Audio envelope, 0–255; zero for non-audio programs |
| 7 | 1 | Attempt sequence, wraps at 255; increments even for a dropped snapshot |
| 8 | 4 | Firmware `millis()` timestamp, unsigned little endian, wraps at 2³² |
| 12 | 48 | 16 consecutive RGB triples, before brightness/correction; **RGB, not wire GRB** |
| 60 | 1 | Detected tempo, 60–200 BPM; zero while acquiring or unlocked |
| 61 | 1 | Tempo correlation score, 0–100; zero while unlocked (not a calibrated probability) |
| 62 | 1 | CRC-8/SMBUS over bytes 0–61: polynomial `0x07`, initial `0`, no reflection or final XOR |

The CRC check value for ASCII `123456789` is `0xf4`. The browser scans for the
magic header, validates version/length/CRC, and advances one byte on invalid
candidates. Serial reads need not align with packets. The parser retains at most
62 pending bytes between reads. Unknown program IDs can still be displayed as
raw LED data; changing the wire layout or LED count requires a protocol revision.

At 30 packets/s the payload is 1,890 bytes/s, about **16% of 115200-baud UART
capacity** with framing bits. A packet takes about 5.5 ms on the wire, transmitted
asynchronously. Sampling every four renders adds up to about 33 ms before USB and
browser scheduling. The dashboard reports time since receipt, not measured
end-to-end latency. `FastLED.show()` temporarily masks AVR interrupts, so actual
UART timing and lamp overhead must still be measured on hardware. No claim of
zero overhead is made.

The 63-byte automatic packet array exists only while sending. Static RAM usage
does not include this stack space. A compile-time assertion ensures a packet fits
the configured Arduino TX ring; the default 64-byte ring has 63 usable slots.
The capacity check is tested with every insufficient capacity and a full packet.

### Program selection (host → lamp)

The dashboard sends six bytes for a program change:

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 2 | Magic bytes `0x4c 0x43` (`LC`) |
| 2 | 1 | Command protocol version: `1` |
| 3 | 1 | Opcode: `1` (select program) |
| 4 | 1 | Program ID, 0–7 from `ProgramList.def` |
| 5 | 1 | Same CRC-8 over bytes 0–4 |

For example, selecting Rainbow sends `4c 43 01 01 07 94`.
The firmware consumes at most eight available bytes per loop, never waiting for
more. Invalid versions, opcodes, IDs and checksums are discarded; the six-byte
parser resynchronizes after noise and expires partial commands after a 100 ms
inter-byte gap. Selecting the current program is a no-op, preserving effect state
and BPM lock. The physical button continues from the currently selected program.
Neither source writes EEPROM; restart still selects Color blocks.

The app permits one pending change and uses the regular telemetry program field
as confirmation. It highlights the lamp's reported program, not an optimistic
local choice. A write failure, disconnect, or lack of confirmation within two
seconds reports failure or cancels the pending change. Stale/missing telemetry
disables program controls. Physical button changes continue to update the app.
Brightness and pixel writes are not implemented.

### Microphone capture (host → lamp → host)

`LC 01 02 01 CRC` enables/renews capture; `LC 01 02 00 CRC` disables it.
The enable argument is strictly 0 or 1. A request expires after **3 seconds**;
the visible dashboard renews it every second only in Sound reactive. Hiding the
tab or switching programs disables capture. Closing the connection stops renewal;
the lease bounds any continued transmission after abrupt disconnection. Old
firmware remains readable and shows an update hint for capture.

The lamp subsamples its existing ADC reads, at most once per **1,000 µs**. It
does not add ADC reads, change beat detection, or impose a new sampling schedule.
Each sample carries its actual acquisition timestamp, the latest smoothed onset
from the detector's 10 ms windows, and the state of the beat clock's 90 ms flash.
This is a time-domain diagnostic stream; subsampling can alias high frequencies
and cannot be used as a full-bandwidth recording or calibrated sound-level meter.

Ten samples form one 60-byte packet, interleaved with regular LED packets:

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 2 | Magic `LA` (`4c 41`) |
| 2 | 1 | Audio packet version `1` |
| 3 | 1 | Sample count `10` |
| 4 | 1 | Attempt sequence; increments on dropped full batches too |
| 5 | 4 | First sample's `micros()`, unsigned little endian, wrapping at 2³² |
| 9 | 50 | Ten consecutive five-byte sample records |
| 59 | 1 | CRC-8 over bytes 0–58 |

Each sample record holds a little-endian uint16 offset in µs from the first
sample, a little-endian uint16 with ADC value in bits 0–9 and beat-active in bit
15 (bits 10–14 reserved zero), then one byte of onset strength (0–255). Offsets
start at zero and strictly increase. Long interruptions discard partial batches.

One static packet buffer bounds memory. Full packets are dropped when the TX
queue is busy; no waiting or backlog is introduced. UART time is reserved before
the next LED snapshot. At the maximum rate, audio uses 6,000 bytes/s; together
with 30 Hz LED snapshots this is about 69% of nominal 115200-baud capacity.
Actual rates are lower due to timing jitter, LED interrupt masking and dropped
batches. Hardware timing still needs measurement.

The parser validates both packet types independently and retains at most 62
partial bytes. The dashboard unwraps device timestamps, leaves gaps for missing
batches and retains only five seconds (at most 5,100 samples). The displayed rate
is the received sample rate including gaps. CSV columns are `time_ms` relative
to the first exported sample, `adc`, `onset`, `beat` and `gap`. A gap marks a new
segment; it is not interpolated data. The simulator emits this same packet format
from the ADC values actually passed to the C++ engine, with the same onset and
beat fields.

### Connection lifecycle

Opening a serial port can toggle DTR and reset a Pro Mini. The browser deasserts
DTR/RTS after opening, but cannot prevent an adapter/driver's initial reset pulse.
Close the dashboard connection before uploading firmware. Older firmware's text
program logs are incompatible; upload the version in this repository first.

The simulator uses the same encoder for its frames but advances the sequence once
per simulated render. Its virtual clock uses the firmware's 8,333 µs interval and
its deterministic square-wave ADC source is sampled at 1 kHz. Tests compare the
entire packet from native C++ and WebAssembly, including button-triggered program
changes and audio input.
