#!/usr/bin/env python3
"""Measure the opt-in timing firmware with LEDs active, without needing music."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import time

import serial
from capture_microphone import command, crc8

FIELDS = ["frames", "min_frame_us", "max_frame_us", "max_work_us", "max_input_us",
          "late_frames", "analysis_busy_windows", "stack_headroom_bytes", "work_calls"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--seconds", type=float, default=45)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--firmware", type=Path, default=Path(".pio/build/nanoatmega328_timing/firmware.hex"))
    args = parser.parse_args()
    if args.seconds < 30:
        parser.error("Record at least 30 seconds to exercise all UART modes")
    firmware_hash = hashlib.sha256(args.firmware.read_bytes()).hexdigest()
    tracker_hash = hashlib.sha256((Path(__file__).resolve().parents[1] / "lib/LampLogic/BeatTracker.h").read_bytes()).hexdigest()
    args.output.mkdir(parents=True, exist_ok=False)
    port = serial.Serial(port=None, baudrate=115200, timeout=.02, write_timeout=1)
    port.dtr = port.rts = False
    port.port = args.port
    pending = bytearray()
    rows, frames = [], []
    window_times = []
    start = renew = time.monotonic()
    try:
        port.open()
        while time.monotonic() - start < args.seconds:
            elapsed = time.monotonic() - start
            mode = min(2, int(3 * elapsed / args.seconds))
            if time.monotonic() >= renew:
                command(port, 1, 2)
                command(port, 2, mode)
                renew = time.monotonic() + 1
            pending.extend(port.read(4096))
            while len(pending) >= 4:
                kind = bytes(pending[:2])
                size = 27 if kind == b"LT" and pending[2:4] == bytes([1, 27]) else \
                       63 if kind == b"LM" else 60 if kind == b"LA" else 0
                if not size:
                    del pending[0]
                    continue
                if len(pending) < size:
                    break
                packet = bytes(pending[:size])
                if crc8(packet[:-1]) != packet[-1]:
                    del pending[0]
                    continue
                del pending[:size]
                if kind == b"LM":
                    frames.append({"device_ms": int.from_bytes(packet[8:12], "little"), "bpm": packet[60]})
                if kind == b"LA" and packet[2] == 2:
                    base = int.from_bytes(packet[5:9], "little")
                    window_times.extend((base + int.from_bytes(packet[9 + i * 5:11 + i * 5], "little")) & 0xffffffff
                                        for i in range(packet[3]))
                if kind != b"LT":
                    continue
                row = {"capture_mode": mode, "device_ms": int.from_bytes(packet[4:8], "little")}
                row.update({name: int.from_bytes(packet[8 + i * 2:10 + i * 2], "little") for i, name in enumerate(FIELDS)})
                rows.append(row)
                print(row, flush=True)
    finally:
        if port.is_open:
            try:
                command(port, 2, 0)
            finally:
                port.close()
    if not rows:
        raise SystemExit("No timing packets received; verify the timing build is installed")
    with (args.output / "timing.csv").open("w") as file:
        writer = csv.DictWriter(file, fieldnames=list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    summary = {
        "source": "physical ATmega328P, synthetic audio stress, LEDs active",
        "firmware_sha256": firmware_hash, "tracker_sha256": tracker_hash,
        "seconds": args.seconds, "reports": len(rows),
        "min_frame_us": min(r["min_frame_us"] for r in rows),
        "max_frame_us": max(r["max_frame_us"] for r in rows),
        "max_work_us": max(r["max_work_us"] for r in rows),
        "max_input_us": max(r["max_input_us"] for r in rows),
        "late_frames": sum(r["late_frames"] for r in rows),
        # A tempo update may span input windows. Sampling continues during it;
        # these are deferred scan starts, not missing microphone measurements.
        "analysis_busy_windows": sum(r["analysis_busy_windows"] for r in rows),
        "max_busy_windows_per_report": max(r["analysis_busy_windows"] for r in rows),
        "recorded_input_windows": len(window_times),
        "input_window_gaps": sum(((b - a) & 0xffffffff) > 15 for a, b in zip(window_times, window_times[1:])),
        "stack_headroom_bytes": min(r["stack_headroom_bytes"] for r in rows),
        "capture_modes": sorted({r["capture_mode"] for r in rows}),
        "work_calls": sum(r["work_calls"] for r in rows),
        "last_bpm": frames[-1]["bpm"] if frames else None,
    }
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2), flush=True)
    # Explicit limits tied to the scheduler's 1 ms / 750 us admission guards.
    if (summary["max_work_us"] >= 900 or summary["max_input_us"] >= 600 or
        summary["late_frames"] or summary["input_window_gaps"] or
        summary["max_busy_windows_per_report"] > 4 or summary["recorded_input_windows"] < 500 or
        summary["stack_headroom_bytes"] < 128 or summary["work_calls"] < 1000 or
        summary["capture_modes"] != [0, 1, 2]):
        raise SystemExit("Timing or memory budget failed; do not deploy this build")


if __name__ == "__main__":
    main()
