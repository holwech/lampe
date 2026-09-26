#!/usr/bin/env python3
"""Temporarily flash 4 kHz diagnostic firmware, record ADC, always restore lamp."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import time

import serial

ROOT = Path(__file__).resolve().parents[2]


def crc8(data):
    result = 0
    for byte in data:
        result ^= byte
        for _ in range(8):
            result = ((result << 1) ^ (7 if result & 128 else 0)) & 255
    return result


def decode(packet):
    if len(packet) != 63 or packet[:6] != b"LP\x01\x18\xa0\x0f" or crc8(packet[:-1]) != packet[-1]:
        raise ValueError("Invalid PCM packet or checksum")
    sequence = int.from_bytes(packet[6:8], "little")
    index = int.from_bytes(packet[8:12], "little")
    dropped = int.from_bytes(packet[12:14], "little")
    samples = [int.from_bytes(packet[i:i + 2], "little") for i in range(14, 62, 2)]
    if dropped or any(v > 1023 for v in samples):
        raise ValueError(f"Invalid ADC data or ring overflow ({dropped} dropped)")
    return sequence, index, samples


def flash(port, firmware):
    subprocess.run([
        "uv", "run", "--locked", "pio", "pkg", "exec", "--package", "platformio/tool-avrdude", "--",
        "avrdude", "-N", "-p", "m328p", "-c", "arduino", "-P", port, "-b", "57600", "-D",
        "-U", f"flash:w:{firmware.resolve()}:i",
    ], cwd=ROOT, check=True)


def record(port_name, duration, output):
    pending = bytearray()
    previous_sequence = previous_index = None
    packets = sample_count = 0
    first_received = last_received = None
    minimum, maximum = 1023, 0
    started = time.monotonic()
    with serial.Serial(port_name, 115200, timeout=0.1) as port, \
         (output / "serial.bin").open("wb") as binary, (output / "adc.csv").open("w") as csv_file:
        writer = csv.writer(csv_file)
        writer.writerow(["sample_index", "adc"])
        while sample_count < round(duration * 4000):
            if time.monotonic() - started > duration + 15:
                raise RuntimeError("Timed out waiting for a complete 4 kHz capture")
            data = port.read(4096)
            binary.write(data)
            pending.extend(data)
            while len(pending) >= 63:
                if pending[:3] != b"LP\x01":
                    if packets:
                        raise ValueError("Lost framing during PCM recording")
                    del pending[0]
                    continue
                packet = bytes(pending[:63])
                del pending[:63]
                sequence, index, samples = decode(packet)
                if previous_sequence is not None and (sequence != ((previous_sequence + 1) & 65535) or index != previous_index + 24):
                    raise ValueError("Missing or reordered PCM packet; recording is incomplete")
                previous_sequence, previous_index = sequence, index
                received = time.monotonic()
                if first_received is None:
                    first_received = received
                last_received = received
                writer.writerows((index + i, v) for i, v in enumerate(samples))
                minimum, maximum = min(minimum, min(samples)), max(maximum, max(samples))
                sample_count += 24
                packets += 1
    return {"sample_rate_hz": 4000, "samples": sample_count, "duration_seconds": sample_count / 4000,
            "packets": packets, "sample_gaps": 0, "rejected_packets": 0, "adc_range": [minimum, maximum],
            "receive_span_seconds": round(last_received - first_received, 4),
            "host_rate_estimate_hz": round((sample_count - 24) / (last_received - first_received), 1)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--seconds", type=float, default=75)
    parser.add_argument("--track", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--firmware", type=Path, required=True)
    parser.add_argument("--restore-firmware", type=Path, required=True)
    args = parser.parse_args()
    if args.seconds <= 0 or not all(p.is_file() for p in [args.firmware, args.restore_firmware]):
        parser.error("Positive duration and both existing firmware images are required")
    if args.firmware.resolve() == args.restore_firmware.resolve():
        parser.error("Capture firmware and restoration image must be different")
    args.output.mkdir(parents=True, exist_ok=False)
    metadata = {"track": args.track, "source": "physical lamp ADC0, temporary timer-triggered diagnostic firmware",
                "led_updates_during_capture": False, "valid": False, "firmware_restored": False,
                "capture_firmware_sha256": hashlib.sha256(args.firmware.read_bytes()).hexdigest(),
                "restore_firmware_sha256": hashlib.sha256(args.restore_firmware.read_bytes()).hexdigest()}
    try:
        flash(args.port, args.firmware)
        metadata.update(record(args.port, args.seconds, args.output))
        metadata["valid"] = True
    finally:
        try:
            flash(args.port, args.restore_firmware)
            metadata["firmware_restored"] = True
        finally:
            (args.output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps(metadata, indent=2))


if __name__ == "__main__":
    main()
