#!/usr/bin/env python3
"""Audit real detector windows and ADC cadence; always restore normal firmware."""
import argparse
import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import time

import serial

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from capture_microphone import command, crc8
from measure_timing import FIELDS as TIMING_FIELDS

spec = importlib.util.spec_from_file_location("pcm_capture", ROOT / "experiments/microphone-pcm/capture.py")
pcm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pcm)

FIELDS = [("sequence", 4, 2), ("device_ms", 6, 4), ("first_us", 10, 4),
          ("last_us", 14, 4), ("count", 18, 2), ("min_gap_us", 20, 2),
          ("max_gap_us", 22, 2), ("minimum", 24, 2), ("maximum", 26, 2),
          ("peak", 28, 2), ("sum", 30, 4), ("onset", 34, 1), ("bpm", 35, 1),
          ("confidence", 36, 1), ("busy_samples", 37, 1), ("dropped", 38, 2), ("led_level", 40, 1)]


def decode(packet):
    if len(packet) != 42 or packet[:4] != b"LS\x01\x2a" or crc8(packet[:-1]) != packet[-1]:
        raise ValueError("Invalid sampling packet")
    row = {name: int.from_bytes(packet[at:at + size], "little") for name, at, size in FIELDS}
    if not row["count"] or not 0 <= row["minimum"] <= row["maximum"] <= 1023:
        raise ValueError("Invalid ADC statistics")
    if row["peak"] != row["maximum"] - row["minimum"]:
        raise ValueError("Window statistics differ from actual detector input")
    if not row["minimum"] * row["count"] <= row["sum"] <= row["maximum"] * row["count"]:
        raise ValueError("Invalid ADC sum")
    return row


def record(port_name, output, seconds):
    pending = bytearray()
    rows, timing = [], []
    sequence = last_ms = None
    rejected = missing = gaps = 0
    start = renew = time.monotonic()
    host_start = time.time()
    first_received = None
    with serial.Serial(port_name, 115200, timeout=.02, write_timeout=1) as port, \
            (output / "serial.bin").open("wb") as binary, \
            (output / "sampling.csv").open("w") as sampling_file, \
            (output / "windows.csv").open("w") as windows_file, \
            (output / "timing.csv").open("w") as timing_file:
        sampling = csv.DictWriter(sampling_file, ["host_s"] + [f[0] for f in FIELDS], lineterminator="\n")
        windows = csv.writer(windows_file, lineterminator="\n")
        timing_writer = csv.DictWriter(timing_file, ["device_ms"] + TIMING_FIELDS, lineterminator="\n")
        sampling.writeheader(); windows.writerow(["device_ms", "peak", "onset"]); timing_writer.writeheader()
        while first_received is None or time.monotonic() - first_received < seconds:
            if time.monotonic() - start > seconds + 15:
                raise TimeoutError("No complete diagnostic stream")
            if time.monotonic() >= renew:
                command(port, 1, 2)
                renew = time.monotonic() + 1
            chunk = port.read(4096)
            binary.write(chunk)
            pending.extend(chunk)
            while len(pending) >= 4:
                prefix = bytes(pending[:4])
                size = {b"LS\x01\x2a": 42, b"LT\x01\x1b": 27, b"LM\x04\x10": 63}.get(prefix, 0)
                if not size:
                    del pending[0]
                    continue
                if len(pending) < size:
                    break
                packet = bytes(pending[:size])
                if crc8(packet[:-1]) != packet[-1]:
                    rejected += 1
                    del pending[0]
                    continue
                del pending[:size]
                if packet[:2] == b"LS":
                    row = decode(packet)
                    row["host_s"] = round(time.monotonic() - start, 6)
                    if sequence is not None:
                        missing += (row["sequence"] - sequence - 1) & 65535
                        gaps += ((row["device_ms"] - last_ms) & 0xffffffff) > 15
                    sequence, last_ms = row["sequence"], row["device_ms"]
                    rows.append(row)
                    sampling.writerow(row)
                    windows.writerow([row["device_ms"], row["peak"], row["onset"]])
                    if first_received is None:
                        first_received = time.monotonic()
                        (output / "ready.json").write_text(json.dumps({"host_unix_s": time.time(), "device_ms": last_ms}))
                        print(f"RECORDER READY: collecting for {seconds:g} seconds", flush=True)
                elif packet[:2] == b"LT":
                    row = {"device_ms": int.from_bytes(packet[4:8], "little")}
                    row.update({name: int.from_bytes(packet[8 + i*2:10 + i*2], "little") for i, name in enumerate(TIMING_FIELDS)})
                    timing.append(row); timing_writer.writerow(row)
    print("RECORDING FINISHED — pause playback now.", flush=True)
    return dict(mode="windows", host_started_unix_s=host_start, seconds=seconds,
                windows=len(rows), rejected_packets=rejected, missing_packets=missing,
                window_gaps=gaps, firmware_dropped=max(r["dropped"] for r in rows),
                timing_reports=len(timing), late_frames=sum(r["late_frames"] for r in timing),
                max_frame_us=max(r["max_frame_us"] for r in timing),
                max_input_us=max(r["max_input_us"] for r in timing),
                max_work_us=max(r["max_work_us"] for r in timing),
                stack_headroom_bytes=min(r["stack_headroom_bytes"] for r in timing))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--seconds", type=float, default=85)
    parser.add_argument("--condition", choices=["steady", "pulsed"], required=True)
    parser.add_argument("--track", required=True)
    parser.add_argument("--reference-bpm", type=float)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--firmware", type=Path, required=True)
    parser.add_argument("--restore-firmware", type=Path, required=True)
    args = parser.parse_args()
    if args.seconds < 5 or not all(p.is_file() for p in [args.firmware, args.restore_firmware]):
        parser.error("Record at least five seconds and supply two existing firmware images")
    if args.firmware.resolve() == args.restore_firmware.resolve():
        parser.error("Diagnostic and normal firmware must differ")
    args.output.mkdir(parents=True, exist_ok=False)
    backup = args.output.resolve() / "restore.hex"
    backup.write_bytes(args.restore_firmware.read_bytes())
    metadata = dict(condition=args.condition, track=args.track, reference_bpm=args.reference_bpm,
                    firmware_sha256=hashlib.sha256(args.firmware.read_bytes()).hexdigest(),
                    restore_sha256=hashlib.sha256(backup.read_bytes()).hexdigest(), restored=False)
    try:
        pcm.flash(args.port, args.firmware)
        metadata.update(record(args.port, args.output, args.seconds))
    finally:
        print("Capture ended. Restoring normal lamp firmware.", flush=True)
        try:
            pcm.flash(args.port, backup)
            metadata["restored"] = True
        finally:
            (args.output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps(metadata, indent=2), flush=True)


if __name__ == "__main__":
    main()
