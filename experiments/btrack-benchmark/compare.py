#!/usr/bin/env python3
"""Compare unchanged BTrack with the lamp using the same recorded onset inputs."""
import argparse
import csv
import hashlib
import io
import json
import math
from pathlib import Path
import random
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "tests/fixtures/microphone"


def replay(executable, samples, initial):
    data = "time_ms,peak\n" + "".join(f"{t},{p}\n" for t, p in samples)
    command = [str(executable)] + ([] if initial == "default" else [initial])
    output = subprocess.run(command, input=data, text=True, capture_output=True, check=True).stdout
    rows = [{k: float(v) for k, v in row.items()} for row in csv.DictReader(io.StringIO(output))]
    assert len(rows) == len(samples)
    assert all(row["time_ms"] == sample[0] and row["peak"] == sample[1]
               for row, sample in zip(rows, samples))
    assert all(math.isfinite(v) for row in rows for v in row.values())
    assert all(row["btrack_ticks"] == row["segment_ms"] // 10 + 1 for row in rows)
    return output, rows


def summarize(rows, reference=None, tolerance=4, warmup=10000):
    start = rows[0]["time_ms"]
    evaluated = [r for r in rows if r["time_ms"] - start >= warmup]
    assert evaluated
    result = {"windows": len(rows), "segments": int(rows[-1]["segment"]),
              "evaluated_windows": len(evaluated)}
    for tracker, pulse in [("lamp", "lamp_pulse"), ("btrack", "btrack_beat")]:
        values = [r[f"{tracker}_bpm"] for r in evaluated]
        positive = [v for v in values if v > 0]
        stats = {"nonzero_tempo_percent": round(100 * len(positive) / len(values), 2),
                 "tempo_range": [round(min(positive), 2), round(max(positive), 2)] if positive else None}
        beats = [r["time_ms"] for i, r in enumerate(rows)
                 if r[pulse] > 0 and (tracker == "btrack" or i == 0 or rows[i - 1][pulse] == 0)]
        stats["beat_events_including_startup"] = len(beats)
        if reference is not None:
            stats["near_reference_percent"] = round(100 * sum(abs(v - reference) <= tolerance for v in values) / len(values), 2)
            stats["outside_reference_percent"] = round(100 * sum(v > 0 and abs(v - reference) > tolerance for v in values) / len(values), 2)
            # Sensitivity to fresh-start behavior after diagnostic gaps.
            stable = [r[f"{tracker}_bpm"] for r in evaluated if r["segment_ms"] >= warmup]
            stats["near_reference_after_each_segment_warmup_percent"] = round(
                100 * sum(abs(v - reference) <= tolerance for v in stable) / len(stable), 2) if stable else None
        result[tracker] = stats
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    cases = json.loads((FIXTURES / "manifest.json").read_text())
    versions = json.loads(subprocess.check_output([str(args.executable.resolve()), "--info"], text=True))
    assert versions["lamp_header_sha256"] == hashlib.sha256((ROOT / "lib/LampLogic/BeatTracker.h").read_bytes()).hexdigest(), "Rebuild the benchmark after changing BeatTracker.h"
    results = {**versions,
               "lamp_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
               "onset_hz": 100, "resampling": "causal zero-order hold; reset both trackers at gaps >15ms",
               "warmup_ms": 10000, "initializations": ["default", "100", "150"], "cases": []}
    inputs = []
    for case in cases:
        with (FIXTURES / case["file"]).open() as file:
            samples = [(int(r["time_ms"]), int(r["peak"])) for r in csv.DictReader(file)]
        inputs.append((Path(case["file"]).stem, samples, case.get("reference_bpm"), "recording"))
    for value in [0, 4, 100]:
        inputs.append((f"constant-{value}", [(t, value) for t in range(10, 60001, 10)], None, "negative"))
    source = inputs[0][1]
    for seed in range(20):
        blocks = [source[i:i + 5] for i in range(0, len(source), 5)]
        random.Random(seed).shuffle(blocks)
        peaks = [p for block in blocks for _, p in block]
        inputs.append((f"shuffle-{seed:02d}", [(10 * (i + 1), p) for i, p in enumerate(peaks)], None, "negative"))
    for tempo in [60, 80, 100, 120, 136, 150, 160, 173, 200]:
        period = 60000 / tempo
        samples = [(t, 180 if t >= 1000 and (t - 1000) % period < 20 else 4)
                   for t in range(10, 60001, 10)]
        inputs.append((f"synthetic-{tempo}", samples, tempo, "synthetic"))
    for name, samples, reference, kind in inputs:
        item = {"name": name, "kind": kind, "reference_bpm": reference, "runs": {}}
        for initial in results["initializations"]:
            output, rows = replay(args.executable.resolve(), samples, initial)
            (args.output / f"{name}-{initial}.csv").write_text(output)
            summary = summarize(rows, reference)
            if kind == "synthetic":
                period = 60000 / reference
                for tracker, pulse in [("lamp", "lamp_pulse"), ("btrack", "btrack_beat")]:
                    times = [r["time_ms"] for i, r in enumerate(rows)
                             if r["time_ms"] >= 10000 and r[pulse] > 0 and
                             (tracker == "btrack" or i == 0 or rows[i - 1][pulse] == 0)]
                    errors = sorted(abs(((t - 1000 + period / 2) % period) - period / 2) for t in times)
                    summary[tracker]["median_beat_error_ms"] = round(errors[len(errors) // 2], 2) if errors else None
                    summary[tracker]["beats_after_10s"] = len(times)
            item["runs"][initial] = summary
        results["cases"].append(item)
        default = item["runs"]["default"]
        print(name, {k: default[k].get("near_reference_percent", default[k]["beat_events_including_startup"])
                     for k in ["lamp", "btrack"]}, flush=True)
    (args.output / "summary.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
