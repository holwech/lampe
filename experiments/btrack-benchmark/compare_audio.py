# /// script
# requires-python = ">=3.14"
# dependencies = ["numpy==2.5.3", "scipy==1.18.1", "matplotlib==3.11.2"]
# ///
"""Compare causal onset front ends on one gap-free 4 kHz lamp ADC recording."""
import argparse
import csv
import io
import json
from pathlib import Path
import subprocess

import numpy as np
from scipy.signal import butter, sosfilt

from compare import summarize


def features(adc):
    blocks = adc[:len(adc) // 40 * 40].reshape(-1, 40)
    peaks = np.ptp(blocks, axis=1).astype(int)
    # Fixed recipes chosen before inspecting their results. All filtering is
    # forward-only, with trailing windows; no zero-phase filter or global gain.
    source = (adc.astype(float) - 512) / 512
    result = {}
    for name, limits in [("bass", [40, 200]), ("mid", [200, 600]), ("high", [600, 1800])]:
        filtered = sosfilt(butter(2, limits, btype="bandpass", fs=4000, output="sos"), source)
        frames = filtered[:len(blocks) * 40].reshape(-1, 40)
        energy = np.sqrt(np.mean(frames ** 2, axis=1))
        compressed = np.log1p(100 * energy)
        result[name] = np.maximum(0, np.diff(compressed, prepend=0))
    result["three-band"] = result["bass"] + result["mid"] + result["high"]
    return peaks, result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("build", type=Path)
    parser.add_argument("--reference-bpm", type=float, help="Evaluation only; omit for a negative control")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    metadata = json.loads((args.capture / "metadata.json").read_text())
    if not metadata["valid"] or metadata["sample_rate_hz"] != 4000 or metadata["sample_gaps"]:
        parser.error("Requires a valid, gap-free 4 kHz diagnostic recording")
    data = np.loadtxt(args.capture / "adc.csv", skiprows=1, delimiter=",", dtype=np.int64)
    assert np.all(np.diff(data[:, 0]) == 1) and np.all((data[:, 1] >= 0) & (data[:, 1] <= 1023))
    peaks, onsets = features(data[:, 1])
    args.output.mkdir(parents=True, exist_ok=False)
    executable = (args.build / "beat_benchmark").resolve()
    feature_tool = (args.build / "audio_features").resolve()
    with (args.capture / "adc.csv").open() as file:
        complex_csv = subprocess.check_output([str(feature_tool)], stdin=file, text=True)
    complex_rows = list(csv.DictReader(io.StringIO(complex_csv)))
    assert len(complex_rows) == len(peaks)
    assert [int(r["peak"]) for r in complex_rows] == list(peaks)
    onsets["complex-spectrum"] = np.array([float(r["onset"]) for r in complex_rows])
    methods = {"lamp-onsets": None, **onsets}
    summaries, all_rows = {}, {}
    for name, onset in methods.items():
        recording = "time_ms,peak" + (",onset" if onset is not None else "") + "\n"
        recording += "".join(f"{(i + 1) * 10},{peak}" + (f",{onset[i]:.8f}" if onset is not None else "") + "\n"
                             for i, peak in enumerate(peaks))
        (args.output / f"{name}-input.csv").write_text(recording)
        summaries[name] = {}
        for initial in ["default", "100", "150"]:
            command = [str(executable)] + ([] if initial == "default" else [initial])
            output = subprocess.check_output(command, input=recording, text=True)
            rows = [{k: float(v) for k, v in r.items()} for r in csv.DictReader(io.StringIO(output))]
            assert len(rows) == len(peaks) and all(np.isfinite(v) for r in rows for v in r.values())
            (args.output / f"{name}-{initial}.csv").write_text(output)
            summaries[name][initial] = summarize(rows, args.reference_bpm)
            if initial == "default":
                all_rows[name] = rows
        metric = "near_reference_percent" if args.reference_bpm is not None else "beat_events_including_startup"
        print(name, metric, {k: round(v["btrack"][metric], 1) for k, v in summaries[name].items()}, flush=True)
    result = {"capture": str(args.capture), "capture_metadata": metadata,
              "versions": json.loads(subprocess.check_output([str(executable), "--info"], text=True)),
              "reference_bpm": args.reference_bpm, "tolerance_bpm": 4,
              "warmup_ms": 10000, "methods": summaries}
    (args.output / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
    with (args.output / "features.csv").open("w") as file:
        writer = csv.writer(file, lineterminator="\n")
        writer.writerow(["time_ms", "peak", *methods])
        for i, peak in enumerate(peaks):
            writer.writerow([(i + 1) * 10, int(peak), int(all_rows["lamp-onsets"][i]["onset"]),
                             *(f"{values[i]:.8f}" for values in onsets.values())])

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(3, 1, figsize=(11, 8), sharex=True, layout="constrained")
    time = np.arange(1, len(peaks) + 1) / 100
    axes[0].plot(time, peaks, color="#627756", linewidth=.6)
    axes[0].set_ylabel("ADC peak-to-peak")
    for name in ["lamp-onsets", "complex-spectrum", "three-band"]:
        axes[1].plot(time, [r["btrack_bpm"] for r in all_rows[name]], label=f"BTrack + {name}", linewidth=1.2)
    axes[1].plot(time, [r["lamp_bpm"] or np.nan for r in all_rows["lamp-onsets"]], label="Lamp detector", color="black")
    if args.reference_bpm is not None:
        axes[1].axhspan(args.reference_bpm - 4, args.reference_bpm + 4, alpha=.12, color="green")
    axes[1].set_ylabel("Estimated BPM")
    axes[1].legend(loc="upper right", fontsize=8)
    for name in ["bass", "mid", "high"]:
        axes[2].plot(time, onsets[name], label=name, linewidth=.5, alpha=.75)
    axes[2].set_ylabel("Band attacks")
    axes[2].set_xlabel("Recording time (seconds)")
    axes[2].legend(loc="upper right")
    subtitle = ("Approximate tempo reference; beat positions have not been annotated"
                if args.reference_bpm is not None else "Negative control; playback paused")
    fig.suptitle(f"{metadata['track']} — same microphone recording, different audio features\n{subtitle}", fontsize=12)
    fig.savefig(args.output / "comparison.png", dpi=150)
    plt.close(fig)


if __name__ == "__main__":
    main()
