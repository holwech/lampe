#!/usr/bin/env python3
"""Replay a local windows.csv through the firmware's actual C++ BPM detector."""

import argparse
import csv
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path, help="Directory created by capture_microphone.py --windows")
    parser.add_argument("--output", type=Path, help="Output CSV; defaults to CAPTURE/replay.csv")
    parser.add_argument("--warmup", type=float, default=10, help="Seconds excluded from summary, default 10")
    parser.add_argument("--tolerance", type=float, default=4, help="BPM tolerance around the optional reference")
    args = parser.parse_args()
    if args.warmup < 0 or args.tolerance < 0:
        parser.error("warmup and tolerance must be nonnegative")
    output = args.output or args.capture / "replay.csv"
    source = args.capture / "windows.csv"
    if output.resolve() == source.resolve():
        parser.error("output cannot overwrite the recording")
    metadata = json.loads((args.capture / "metadata.json").read_text())
    if metadata.get("mode") != "windows":
        parser.error("replay requires a --windows capture; raw packets have gaps")
    with tempfile.TemporaryDirectory(prefix="lampe-replay-") as directory:
        executable = Path(directory) / "replay"
        subprocess.run(shlex.split(os.environ.get("CXX", "c++")) + [
            "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
            "-fsanitize=undefined", "-fno-sanitize-recover=all",
            "-I", str(ROOT / "lib/LampLogic"), str(ROOT / "simulator/beat_replay.cpp"),
            "-o", str(executable),
        ], check=True)
        with source.open() as recorded, output.open("w") as result:
            subprocess.run([str(executable)], stdin=recorded, stdout=result, check=True)
    with output.open() as file:
        rows = list(csv.DictReader(file))
    first = int(rows[0]["device_ms"])
    gaps = sum(((int(b["device_ms"]) - int(a["device_ms"])) & 0xffffffff) > 15
               for a, b in zip(rows, rows[1:]))
    # Unsigned device time also handles a capture spanning millis() rollover.
    selected = [r for r in rows if ((int(r["device_ms"]) - first) & 0xffffffff) >= args.warmup * 1000]
    if not selected:
        parser.error("capture is shorter than the requested warmup")
    locked = [r for r in selected if int(r["bpm"])]
    summary = dict(windows=len(rows), window_gaps=gaps, warmup_seconds=args.warmup,
                   locked_percent=round(100 * len(locked) / len(selected), 1),
                   locked_bpm_range=[min(int(r["bpm"]) for r in locked), max(int(r["bpm"]) for r in locked)] if locked else None)
    reference = metadata.get("reference_bpm")
    if reference:
        matching = sum(abs(int(r["bpm"]) - reference) <= args.tolerance for r in locked)
        summary.update(reference_bpm=reference, tolerance_bpm=args.tolerance,
                       within_reference_percent=round(100 * matching / len(selected), 1))
    print(json.dumps(summary, indent=2))
    print(f"Replay: {output}")


if __name__ == "__main__":
    main()
