#!/usr/bin/env python3
"""Analyze quiet-room LED interference, in uncalibrated 10-bit ADC counts."""
import argparse
import csv
import hashlib
import json
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    source = args.capture / "adc.csv"
    data = np.loadtxt(source, delimiter=",", skiprows=1)
    time, adc, level = data[:, 0] / 1000, data[:, 1], data[:, 2]
    events = json.loads((args.capture / "events.json").read_text())
    metadata = json.loads((args.capture / "metadata.json").read_text())
    if not metadata.get("valid_transport") or len(data) != metadata["samples"] or len(events) != metadata["events"]:
        raise ValueError("Incomplete/corrupt capture; do not treat it as a complete measurement")

    def raw(a, b):
        return adc[np.searchsorted(time, a):np.searchsorted(time, b)]

    def bins(a, b, width=10):
        out = []
        for start in np.arange(a, b - width + .01, width):
            x = raw(start, start + width)
            if len(x) >= 5:
                out.append((start, float(np.mean(x)), float(np.std(x)), float(np.ptp(x)), len(x)))
        return np.array(out).reshape((-1, 5))

    # Robust threshold fixed before examining the experiment: 3 scaled MAD,
    # with floors of 3 ADC counts for mean and 2 for RMS.
    def band(x, floor):
        mid = float(np.median(x))
        return mid, max(floor, float(3 * 1.4826 * np.median(np.abs(x - mid))))

    starts = {stage: min(e["after_us"] for e in events if e["stage"] == stage) / 1000 for stage in range(13)}
    starts[13] = metadata["duration_us"] / 1000
    constants = []
    for stage in [0, 1, 2, 3, 4, 12]:
        b = bins(starts[stage] + 2000, starts[stage + 1])
        constants.append(dict(stage=stage, name=next(e["name"] for e in events if e["stage"] == stage),
                              mean_adc=round(float(np.median(b[:, 1])), 2),
                              median_rms_adc=round(float(np.median(b[:, 2])), 2),
                              median_peak_to_peak_adc=round(float(np.median(b[:, 3])), 2),
                              p95_peak_to_peak_adc=round(float(np.percentile(b[:, 3], 95)), 2)))

    trials = []
    for e in events:
        stage, phase = e["stage"], e["phase"]
        is_step = stage in (5, 6, 7) and phase in (0, 2) and not (phase == 0 and e["cycle"] == 0)
        is_fade = stage in (8, 9, 10) and phase in (4, 5)
        if not (is_step or is_fade): continue
        t0 = e["after_us"] / 1000
        b = bins(t0, t0 + 1400)
        steady = b[b[:, 0] >= t0 + 1000]
        dc, dc_band = band(steady[:, 1], 3)
        rms, rms_band = band(steady[:, 2], 2)
        target_drift = float(np.median(steady[-10:, 1]) - np.median(steady[:10, 1]))
        stable_target = abs(target_drift) <= 3 and abs(float(np.median(steady[-10:, 2]) - np.median(steady[:10, 2]))) <= 2
        ok = (np.abs(b[:, 1] - dc) <= dc_band) & (b[:, 2] <= rms + rms_band)
        settle = None
        for i in range(len(ok) - 9):
            # 100 ms continuously in band; at least 95% of remaining bins agree.
            if stable_target and np.all(ok[i:i+10]) and np.mean(ok[i:]) >= .95:
                settle = round(float(b[i, 0] - t0 + 10), 1)
                break
        early = raw(t0, t0 + 200)
        trials.append(dict(name=e["name"], cycle=e["cycle"],
                          direction="on/up" if phase in (2, 4) else "off/down", time_ms=t0,
                          settle_ms=settle, dc_target_adc=round(dc, 2), dc_band_adc=round(dc_band, 2),
                          stable_target=stable_target, target_drift_adc=round(target_drift, 2),
                          rms_target_adc=round(rms, 2), rms_band_adc=round(rms_band, 2),
                          peak_deviation_first_200ms_adc=round(float(np.max(np.abs(early - dc))), 2),
                          first_adc_after_event_us=round(1000 * (time[np.searchsorted(time, t0)] - t0))))

    summaries = []
    for name in dict.fromkeys(t["name"] for t in trials):
        for direction in ["on/up", "off/down"]:
            rows = [t for t in trials if t["name"] == name and t["direction"] == direction]
            settled = [r["settle_ms"] for r in rows if r["settle_ms"] is not None]
            summaries.append(dict(name=name, direction=direction, trials=len(rows), settled=len(settled),
                                  median_settle_ms=float(np.median(settled)) if settled else None,
                                  max_settle_ms=max(settled) if settled else None,
                                  median_peak_deviation_adc=round(float(np.median([r["peak_deviation_first_200ms_adc"] for r in rows])), 2)))

    activity = []
    for stage in range(5, 12):
        selected = [e for e in events if e["stage"] == stage]
        segments = []
        if stage in (8, 9, 10):
            for e in selected:
                if e["phase"] not in (1, 3): continue
                end = next(v for v in selected if v["cycle"] == e["cycle"] and v["phase"] == (4 if e["phase"] == 1 else 5))
                segments.extend(bins(e["after_us"] / 1000, end["after_us"] / 1000))
        else:
            end = starts[stage + 1]
            segments = bins(starts[stage], end)
        b = np.asarray(segments)
        activity.append(dict(name=selected[0]["name"], condition="during ramps" if stage in (8, 9, 10) else "whole stage",
                             median_rms_adc=round(float(np.median(b[:, 2])), 2),
                             median_peak_to_peak_adc=round(float(np.median(b[:, 3])), 2),
                             p95_peak_to_peak_adc=round(float(np.percentile(b[:, 3], 95)), 2)))

    report = dict(source=str(args.capture), adc_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                  analysis_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  software=dict(numpy=np.__version__, matplotlib=matplotlib.__version__),
                  sampling=dict(adc_range=[int(np.min(adc)), int(np.max(adc))],
                                median_interval_us=float(np.median(np.diff(time))*1000),
                                led_write_us_range=[min(e["after_us"]-e["before_us"] for e in events if e["stage"]), max(e["after_us"]-e["before_us"] for e in events if e["stage"])],
                                first_adc_after_transition_us_range=[min(r["first_adc_after_event_us"] for r in trials),max(r["first_adc_after_event_us"] for r in trials)]),
                  transport=metadata, constants=constants, transitions=summaries,
                  activity=activity, trials=trials,
                  settling_definition="10 ms bins; mean within max(3 ADC counts, 3 scaled MAD) and RMS below target plus max(2 counts, 3 scaled MAD), for 100 ms continuously and >=95% of remaining 1.4 s; target uses 1.0-1.4 s after end of LED write/ramp and must drift <=3 mean / <=2 RMS counts between its first and last 100 ms",
                  settling_resolution_ms=10,
                  limitations=["Uncalibrated ADC counts, not volts or sound pressure", "No readings during LED serialization; actual sample starts recorded", "Time zero is end of FastLED.show, not an oscilloscope measurement of LED latch/current", "Gradual fades consist of 120 Hz steps", "One board and room, no music; mechanism cannot be isolated without electrical probes"])
    (args.output / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
    whole = bins(0, time[-1])
    with (args.output / "windows.csv").open("w") as f:
        w = csv.writer(f, lineterminator="\n"); w.writerow(["time_ms", "mean_adc", "rms_adc", "peak_to_peak_adc", "samples"])
        w.writerows((round(r[0], 2), round(r[1], 3), round(r[2], 3), round(r[3], 3), int(r[4])) for r in whole)

    plt.rcParams.update({"font.size": 10, "axes.spines.top": False, "axes.spines.right": False})
    fig, ax = plt.subplots(3, 1, figsize=(13, 9), constrained_layout=True)
    ax[0].plot(time[::20] / 1000, level[::20], color="#738948", lw=1)
    ax[0].set(ylabel="LED red level\n(before global brightness)", title="Microphone response to controlled LED changes · quiet-room measurement")
    ax[1].plot(whole[:, 0]/1000, whole[:, 1], color="#566e8a", lw=.8)
    ax[1].set(ylabel="Mean ADC\n(10 ms windows)")
    ax[2].plot(whole[:, 0]/1000, whole[:, 3], color="#b05443", lw=.8)
    ax[2].set(ylabel="Microphone peak-to-peak\n(10-bit ADC counts)", xlabel="Seconds")
    for a in ax:
        a.grid(alpha=.2)
        for s in range(13): a.axvline(starts[s]/1000, color="gray", alpha=.25, lw=.6)
    for s, label in [(0,"Off"),(2,"Steady"),(5,"Steps"),(8,"100 ms"),(9,"500 ms"),(10,"2 s fades"),(11,"Pulses"),(12,"Off")]:
        ax[0].text(starts[s]/1000+.3, 275, label, fontsize=8)
    ax[0].set_ylim(-5, 305)
    fig.savefig(args.output / "overview.png", dpi=160); plt.close(fig)

    fig, axes = plt.subplots(2, 3, figsize=(13, 7), constrained_layout=True, sharex=True)
    groups = [("step_0_64", "on/up"), ("step_0_255", "on/up"), ("step_0_255", "off/down"),
              ("fade_100ms", "off/down"), ("fade_500ms", "off/down"), ("fade_2000ms", "off/down")]
    for a, (name, direction) in zip(axes.flat, groups):
        chosen = [r for r in trials if r["name"] == name and r["direction"] == direction]
        traces = []
        for r in chosen:
            x = bins(r["time_ms"], r["time_ms"]+1000)
            a.plot(x[:, 0]-r["time_ms"], x[:, 3], color="#b05443", alpha=.2, lw=.8)
            traces.append(x[:, 3])
        a.plot(np.arange(100)*10+5, np.median(traces, axis=0), color="#813629", lw=1.8)
        a.set(title=f"{name.replace('_',' ')} · {direction}", xlabel="ms after LED write/ramp ends", ylabel="10 ms peak-to-peak ADC")
        a.set_ylim(0, 8)
        a.axhline(constants[0]["p95_peak_to_peak_adc"], color="#738948", ls="--", lw=1, label="Dark baseline 95th percentile")
        a.grid(alpha=.2)
    axes.flat[0].legend(fontsize=8)
    fig.suptitle("Residual microphone signal after a transition · individual trials and median")
    fig.savefig(args.output / "settling.png", dpi=160); plt.close(fig)
    print(json.dumps({k: report[k] for k in ["constants", "transitions", "activity"]}, indent=2))


if __name__ == "__main__": main()
