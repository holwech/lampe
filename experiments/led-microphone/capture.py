#!/usr/bin/env python3
"""Capture the open-loop LED/microphone experiment and restore normal firmware."""
import argparse
import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import time

import serial

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("pcm_capture", ROOT / "experiments/microphone-pcm/capture.py")
pcm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pcm)

STAGES = ["dark_no_writes", "dark_120hz", "red_64", "red_255", "white_64",
          "step_0_64", "step_64_128", "step_0_255", "fade_100ms", "fade_500ms",
          "fade_2000ms", "beat_pulse_90ms", "dark_recovery"]


def decode(packet):
    if packet[:3] not in (b"LF\x01", b"LE\x01") or pcm.crc8(packet[:-1]) != packet[-1]:
        raise ValueError("Invalid packet/checksum")
    if packet[:2] == b"LF":
        if len(packet) != 61 or not 1 <= packet[3] <= 12:
            raise ValueError("Invalid sample packet size")
        base = int.from_bytes(packet[6:10], "little")
        records = []
        for i in range(packet[3]):
            packed = int.from_bytes(packet[12 + 4*i:16 + 4*i], "little")
            records.append(((base + 4 * ((packed >> 10) & 16383)) & 0xffffffff,
                            packed & 1023, packed >> 24))
        return {"kind": "samples", "sequence": int.from_bytes(packet[4:6], "little"),
                "dropped": int.from_bytes(packet[10:12], "little"), "records": records}
    if len(packet) != 25 or packet[3] != 25:
        raise ValueError("Invalid event size")
    return dict(kind="event", stage=packet[4], cycle=packet[5], phase=packet[6],
                level=packet[7], color=packet[8], brightness=packet[9], before_us=int.from_bytes(packet[10:14], "little"),
                after_us=int.from_bytes(packet[14:18], "little"), frames=int.from_bytes(packet[18:22], "little"),
                dropped=int.from_bytes(packet[22:24], "little"))


def record(port_name, output, limit=None):
    pending = bytearray()
    started = False
    previous_sequence = previous_time = None
    host_start = time.monotonic()
    samples = rejected = missing = dropped = max_gap = 0
    brightness = None
    events = []
    with serial.Serial(port_name, 115200, timeout=.05, write_timeout=1) as port, \
            (output / "serial.bin").open("wb") as binary, \
            (output / "adc.csv").open("w") as csv_file:
        writer = csv.writer(csv_file, lineterminator="\n")
        writer.writerow(["time_us", "adc", "level"])
        while time.monotonic() - host_start < 210:
            chunk = port.read(4096)
            binary.write(chunk)
            pending.extend(chunk)
            while len(pending) >= 4:
                size = 61 if pending[:3] == b"LF\x01" else 25 if pending[:3] == b"LE\x01" else 0
                if not size:
                    del pending[0]
                    continue
                if len(pending) < size:
                    break
                try:
                    data = decode(pending[:size])
                except ValueError:
                    rejected += 1
                    del pending[0]
                    continue
                del pending[:size]
                dropped = max(dropped, data["dropped"])
                if data["kind"] == "samples":
                    if previous_sequence is not None:
                        missing += (data["sequence"] - previous_sequence - 1) & 65535
                    previous_sequence = data["sequence"]
                    for timestamp, adc, level in data["records"]:
                        if previous_time is not None:
                            delta = (timestamp - previous_time) & 0xffffffff
                            if not 0 < delta < 1000000:
                                raise ValueError("Nonmonotonic sample time")
                            max_gap = max(max_gap, delta)
                        previous_time = timestamp
                        writer.writerow([timestamp, adc, level])
                        samples += 1
                    if limit and previous_time >= limit * 1e6:
                        return dict(pilot=True, brightness=brightness, samples=samples, rejected_packets=rejected,
                                    missing_packets=missing, firmware_dropped=dropped, max_sample_gap_us=max_gap)
                elif data["stage"] == 254:
                    brightness = data["brightness"]
                    if not started:
                        port.write(b"G")
                        started = True
                        print("Recording started", flush=True)
                elif data["stage"] == 255:
                    (output / "events.json").write_text(json.dumps(events, indent=2) + "\n")
                    return dict(brightness=brightness, samples=samples, rejected_packets=rejected, missing_packets=missing,
                                firmware_dropped=dropped, max_sample_gap_us=max_gap,
                                duration_us=data["after_us"], scheduled_frames=data["frames"], events=len(events))
                else:
                    if data["stage"] >= len(STAGES):
                        raise ValueError("Unknown stage")
                    data["name"] = STAGES[data["stage"]]
                    events.append(data)
                    if data["cycle"] == data["phase"] == 0:
                        print(data["name"], flush=True)
        raise TimeoutError("Experiment did not finish")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--firmware", type=Path, required=True)
    parser.add_argument("--restore-firmware", type=Path, required=True)
    parser.add_argument("--already-installed", action="store_true")
    parser.add_argument("--pilot-seconds", type=float)
    args = parser.parse_args()
    firmware = args.firmware.resolve()
    restore = args.restore_firmware.resolve()
    if not firmware.is_file() or not restore.is_file() or firmware == restore:
        parser.error("Two different, existing firmware images are required")
    # Preserve restoration bytes outside PlatformIO's disposable build folders.
    restore_bytes = restore.read_bytes()
    args.output.mkdir(parents=True, exist_ok=False)
    backup = args.output.resolve() / "restore.hex"
    backup.write_bytes(restore_bytes)
    report = dict(firmware_sha256=hashlib.sha256(firmware.read_bytes()).hexdigest(),
                  restore_firmware_sha256=hashlib.sha256(restore_bytes).hexdigest(),
                  restored=False, nominal_sample_interval_us=500,
                  nominal_led_frame_interval_us=8333)
    try:
        if not args.already_installed:
            pcm.flash(args.port, firmware)
        report.update(record(args.port, args.output, args.pilot_seconds))
        print("RECORDING FINISHED. Room no longer needs to stay quiet; restoring normal firmware.", flush=True)
        report["valid_transport"] = not any(report[k] for k in ("rejected_packets", "missing_packets", "firmware_dropped"))
    finally:
        try:
            pcm.flash(args.port, backup)
            report["restored"] = True
        finally:
            (args.output / "metadata.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


if __name__ == "__main__":
    main()
