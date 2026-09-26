# Lamp telemetry, version 1

The firmware emits binary snapshots at **115200 baud, 8N1**, with no flow control
or command channel. It attempts one packet after every four rendered LED frames
(about 30 Hz). Only `Lampe::update()` writes to serial. It checks that the entire
packet fits in the AVR UART transmit queue before encoding/writing; a busy queue
causes that snapshot to be dropped. There is no retry, acknowledgement, connection
handshake, allocation, or dependence on browser activity in the lamp loop.

Each packet is exactly 61 bytes:

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 2 | Magic bytes `0x4c 0x4d` (`LM`) |
| 2 | 1 | Version: `1` |
| 3 | 1 | LED count: `16` |
| 4 | 1 | Program ID, indexed from zero in `ProgramList.def` |
| 5 | 1 | Global brightness, 0–255 |
| 6 | 1 | Audio envelope, 0–255; zero for non-audio programs |
| 7 | 1 | Attempt sequence, wraps at 255; increments even for a dropped snapshot |
| 8 | 4 | Firmware `millis()` timestamp, unsigned little endian, wraps at 2³² |
| 12 | 48 | 16 consecutive RGB triples, before brightness/correction; **RGB, not wire GRB** |
| 60 | 1 | CRC-8/SMBUS over bytes 0–59: polynomial `0x07`, initial `0`, no reflection or final XOR |

The CRC check value for ASCII `123456789` is `0xf4`. The browser scans for the
magic header, validates version/length/CRC, and advances one byte on invalid
candidates. Serial reads need not align with packets. The parser retains at most
60 pending bytes between reads. Unknown program IDs can still be displayed as
raw LED data; changing the wire layout or LED count requires a protocol revision.

At 30 packets/s the payload is 1,830 bytes/s, about **16% of 115200-baud UART
capacity** with framing bits. A packet takes about 5.3 ms on the wire, transmitted
asynchronously. Sampling every four renders adds up to about 33 ms before USB and
browser scheduling. The dashboard reports time since receipt, not measured
end-to-end latency. `FastLED.show()` temporarily masks AVR interrupts, so actual
UART timing and lamp overhead must still be measured on hardware. No claim of
zero overhead is made.

The 61-byte automatic packet array exists only while sending. Static RAM usage
does not include this stack space. A compile-time assertion ensures a packet fits
the configured Arduino TX ring; the default 64-byte ring has 63 usable slots.
The capacity check is tested with every insufficient capacity and a full packet.

Live mode is read-only. It never sends brightness, program, or pixel commands.
Opening a serial port can toggle DTR and reset a Pro Mini. The browser deasserts
DTR/RTS after opening, but cannot prevent an adapter/driver's initial reset pulse.
Close the dashboard connection before uploading firmware. Older firmware's text
program logs are incompatible; upload the version in this repository first.

The simulator uses the same encoder for its frames but advances the sequence once
per simulated render. Its virtual clock uses the firmware's 8,333 µs interval and
its deterministic square-wave ADC source is sampled at 1 kHz. Tests compare the
entire packet from native C++ and WebAssembly, including button-triggered program
changes and audio input.
