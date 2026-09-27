#!/usr/bin/env python3
"""Compare paired production-loop captures aligned by their microphone envelopes."""
import argparse
import csv
import gzip
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tempfile

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    with path.open() as file:
        rows = list(csv.DictReader(file))
    if not rows:
        raise ValueError(f"Empty recording: {path}")
    return {k: np.array([float(r[k]) for r in rows]) for k in rows[0]}


def load(directory, executable):
    metadata = json.loads((directory / "metadata.json").read_text())
    playback = json.loads((directory / "playback.json").read_text())
    if any(metadata[k] for k in ["rejected_packets", "missing_packets", "window_gaps", "firmware_dropped"]):
        raise ValueError(f"Incomplete transport: {directory}")
    rows = read(directory / "sampling.csv")
    with (directory / "windows.csv").open() as source, (directory / "replay.csv").open("w") as output:
        subprocess.run([str(executable)], stdin=source, stdout=output, check=True)
    replay = read(directory / "replay.csv")
    offset = np.median(rows["host_s"] - rows["device_ms"] / 1000)
    start = playback["afterPlayUnixMs"] / 1000 - metadata["host_started_unix_s"]
    rows["song_s"] = rows["device_ms"] / 1000 + offset - start
    rows["replay_bpm"] = replay["bpm"]
    return metadata, playback, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", nargs="+", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if len(args.captures) < 2:
        parser.error("Supply at least two captures")
    args.output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        executable = Path(tmp) / "replay"
        subprocess.run(["c++", "-std=c++17", "-O2", "-I", str(ROOT / "lib/LampLogic"),
                        str(ROOT / "simulator/beat_replay.cpp"), "-o", str(executable)], check=True)
        takes = [load(p, executable) for p in args.captures]
        budget_executable = Path(tmp) / "replay_budget"
        subprocess.run(["c++", "-std=c++17", "-O2", "-I", str(ROOT / "lib/LampLogic"),
                        str(Path(__file__).with_name("replay_budget.cpp")), "-o", str(budget_executable)], check=True)
        for (_, _, rows), directory in zip(takes, args.captures):
            for budget in [8, 12, 16, 28]:
                run = subprocess.run([str(budget_executable), str(budget)], input=(directory / "windows.csv").read_text(),
                                     text=True, capture_output=True, check=True)
                rows[f"budget_{budget}"] = np.array([int(r["bpm"]) for r in csv.DictReader(io.StringIO(run.stdout))])
    grid = np.arange(5, 70, .01)
    def envelope(rows, shift=0):
        y = np.interp(grid, rows["song_s"] + shift, rows["peak"])
        return np.convolve(y, np.ones(10) / 10, mode="same")
    a = envelope(takes[0][2])
    shifts = np.arange(-2, 2.001, .01)
    result = dict(alignment=[], takes=[])
    for take, directory in zip(takes[1:], args.captures[1:]):
        correlations = [np.corrcoef(a[200:-200], envelope(take[2], shift)[200:-200])[0, 1] for shift in shifts]
        best = int(np.argmax(correlations))
        shift = shifts[best]
        take[2]["song_s"] += shift
        result["alignment"].append(dict(take=directory.name, shift_s=round(float(shift), 3),
                                        envelope_correlation=round(float(correlations[best]), 4)))
    fig, axes = plt.subplots(4, 1, figsize=(12, 10), sharex=True, constrained_layout=True)
    colors = ["#326694", "#bf643a", "#398061", "#83659b"]
    for i, ((metadata, playback, rows), directory) in enumerate(zip(takes, args.captures)):
        color = colors[i % len(colors)]
        label = metadata["condition"] + (" repeat" if i >= 2 else "")
        selected = (rows["song_s"] >= 15) & (rows["song_s"] <= 70)
        summary = dict(metadata, playback=playback, analyzed_song_seconds=[15, 70])
        summary["take"] = directory.name
        timing = read(directory / "timing.csv")
        summary["frames_per_report_range"] = [int(timing["frames"].min()), int(timing["frames"].max())]
        summary["min_frame_us"] = int(timing["min_frame_us"].min())
        intervals = np.diff(rows["device_ms"])[selected[1:]]
        summary["mean_window_interval_ms"] = round(float(np.mean(intervals)), 5)
        summary["window_interval_ms_counts"] = {str(int(v)): int(np.sum(intervals == v)) for v in np.unique(intervals)}
        summary["largest_sample_gap_us"] = int(rows["max_gap_us"][selected].max())
        summary["sample_count_range"] = [int(rows["count"][selected].min()), int(rows["count"][selected].max())]
        for field in ["count", "min_gap_us", "max_gap_us", "peak", "busy_samples"]:
            summary[field + "_p05_median_p95"] = np.percentile(rows[field][selected], [5, 50, 95]).tolist()
        summary["adc_range"] = [int(rows["minimum"][selected].min()), int(rows["maximum"][selected].max())]
        summary["adc_clipped_windows"] = int(np.sum(selected & ((rows["minimum"] == 0) | (rows["maximum"] == 1023))))
        for field in ["bpm", "replay_bpm"]:
            values = rows[field][selected]
            matching = np.abs(values - metadata["reference_bpm"]) <= 4
            summary[field] = dict(matching_percent=round(float(100*np.mean(matching)), 1),
                                 wrong_percent=round(float(100*np.mean((values > 0) & ~matching)), 1),
                                 locked_range=[int(v) for v in [values[values > 0].min(), values.max()]] if np.any(values) else None)
        summary["live_replay_bpm_disagreement_percent"] = round(float(100*np.mean(rows["bpm"][selected] != rows["replay_bpm"][selected])), 1)
        replay = read(directory / "replay.csv")
        summary["live_replay_onset_disagreements"] = int(np.sum(rows["onset"] != replay["onset"]))
        summary["coarse_work_budget_probe"] = {
            str(budget): dict(live_agreement_within_1_bpm_percent=round(float(100*np.mean(
                np.abs(rows["bpm"][selected] - rows[f"budget_{budget}"][selected]) <= 1)), 1))
            for budget in [8, 12, 16, 28]}
        summary["source_sha256"] = {name: hashlib.sha256((directory / name).read_bytes()).hexdigest()
                                      for name in ["sampling.csv", "windows.csv", "timing.csv", "serial.bin"]}
        result["takes"].append(summary)
        axes[0].plot(grid, envelope(rows), color=color, label=label, alpha=.8)
        for ax, field in zip(axes[1:], ["count", "bpm", "replay_bpm"]):
            ax.plot(rows["song_s"], rows[field], color=color, label=label, alpha=.7, linewidth=.8)
        target = args.output / label.replace(" ", "-")
        target.mkdir(exist_ok=True)
        for name in ["sampling.csv", "windows.csv", "timing.csv", "replay.csv"]:
            (target / (name + ".gz")).write_bytes(gzip.compress((directory / name).read_bytes(), mtime=0))
    for ax, label in zip(axes, ["Peak-to-peak ADC\n100 ms smoothing", "ADC samples / window", "Live BPM", "Replay BPM"]):
        ax.set_ylabel(label); ax.grid(alpha=.2); ax.set_xlim(0, 75); ax.legend(loc="upper right")
    for ax in axes[2:]:
        ax.axhline(136, color="black", linestyle="--", linewidth=.8); ax.set_ylim(0, 205)
    axes[-1].set_xlabel("Song time from playback command (s), subsequent takes aligned by envelope")
    fig.suptitle("Radioactive: same opening passage, production sampling loop")
    fig.savefig(args.output / "comparison.png", dpi=160)
    (args.output / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
