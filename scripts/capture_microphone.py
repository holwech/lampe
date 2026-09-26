#!/usr/bin/env python3
"""Record real lamp telemetry locally, with exact device timestamps and packet gaps."""
import argparse
import csv
import json
from pathlib import Path
import time

import serial


def crc8(data):
    value = 0
    for byte in data:
        value ^= byte
        for _ in range(8):
            value = ((value << 1) ^ (7 if value & 128 else 0)) & 255
    return value


def command(port, opcode, value):
    data = bytes([76, 67, 1, opcode, value])
    port.write(data + bytes([crc8(data)]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--seconds", type=float, default=60)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--reference-bpm", type=float)
    parser.add_argument("--windows", action="store_true", help="Record complete detector peak windows using diagnostic firmware")
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error("--seconds must be positive")
    if args.reference_bpm is not None and args.reference_bpm <= 0:
        parser.error("--reference-bpm must be positive")
    args.output.mkdir(parents=True, exist_ok=False)
    port = serial.Serial(port=None, baudrate=115200, timeout=0.05, write_timeout=1)
    port.dtr = port.rts = False
    port.port = args.port
    pending = bytearray()
    raw_count = window_count = frame_count = rejected = missed = 0
    last_window = None
    window_gaps = 0
    max_window_interval = 0
    sequence = None
    last_us = None
    unwrapped_us = 0
    started = time.monotonic()
    try:
        port.open()
        command(port, 1, 2)
        capture_mode = 2 if args.windows else 1
        command(port, 2, capture_mode)
        renew = started + 1
        with (args.output / "serial.bin").open("wb") as binary, \
             (args.output / "raw.csv").open("w") as raw_file, \
             (args.output / "frames.csv").open("w") as frame_file, \
             (args.output / "windows.csv").open("w") as window_file:
            raw = csv.writer(raw_file)
            frames = csv.writer(frame_file)
            windows = csv.writer(window_file)
            windows.writerow(["device_ms", "peak", "onset"])
            raw.writerow(["time_us", "adc", "onset", "beat", "gap"])
            frames.writerow(["host_s", "device_ms", "program", "audio", "bpm", "confidence", "sequence"])
            while time.monotonic() - started < args.seconds:
                if time.monotonic() >= renew:
                    command(port, 2, capture_mode)
                    command(port, 1, 2)  # Idempotent; also recovers a reset on open.
                    renew = time.monotonic() + 1
                chunk = port.read(4096)
                binary.write(chunk)
                pending.extend(chunk)
                while len(pending) >= 3:
                    kind = bytes(pending[:2])
                    size = 63 if kind == b"LM" and pending[2] in (2, 3, 4) else 60 if kind == b"LA" and pending[2] in (1, 2) else 0
                    if not size:
                        del pending[0]
                        continue
                    if len(pending) < size:
                        break
                    packet = bytes(pending[:size])
                    if packet[3] != (16 if kind == b"LM" else 10) or crc8(packet[:-1]) != packet[-1]:
                        rejected += 1
                        del pending[0]
                        continue
                    del pending[:size]
                    if kind == b"LM":
                        frames.writerow([round(time.monotonic() - started, 6), int.from_bytes(packet[8:12], "little"), packet[4], packet[6], packet[60], packet[61], packet[7]])
                        frame_count += 1
                    else:
                        gap = sequence is not None and packet[4] != ((sequence + 1) & 255)
                        if gap:
                            missed += (packet[4] - sequence - 1) & 255
                        sequence = packet[4]
                        base = int.from_bytes(packet[5:9], "little")
                        for i in range(packet[3]):
                            at = 9 + i * 5
                            us = (base + int.from_bytes(packet[at:at + 2], "little")) & 0xffffffff
                            value = int.from_bytes(packet[at + 2:at + 4], "little")
                            if packet[2] == 2:
                                windows.writerow([us, value, packet[at + 4]])
                                window_count += 1
                                if last_window is not None:
                                    interval = (us - last_window) & 0xffffffff
                                    max_window_interval = max(max_window_interval, interval)
                                    window_gaps += interval > 15
                                last_window = us
                                continue
                            if last_us is not None:
                                unwrapped_us += (us - last_us) & 0xffffffff
                            last_us = us
                            raw.writerow([unwrapped_us, value & 1023, packet[at + 4], int(bool(value & 0x8000)), int(gap and i == 0)])
                            raw_count += 1
    finally:
        if port.is_open:
            try:
                command(port, 2, 0)
            finally:
                port.close()
    metadata = dict(seconds=round(time.monotonic() - started, 3), raw_samples=raw_count,
                    detector_windows=window_count, window_gaps=window_gaps, max_window_interval_ms=max_window_interval,
                    led_frames=frame_count, rejected_packets=rejected, missing_audio_packets=missed,
                    reference_bpm=args.reference_bpm, source="physical lamp microphone", mode="windows" if args.windows else "raw")
    (args.output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps(metadata, indent=2))
    if not (window_count if args.windows else raw_count):
        raise SystemExit("No requested microphone data received. Check the firmware and USB connection.")


if __name__ == "__main__":
    main()
