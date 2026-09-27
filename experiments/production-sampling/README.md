# Production-loop microphone sampling audit

This diagnostic runs the actual ADC, button/serial polling, renderer, beat
tracker and cooperative work scheduler. It replaces only the final pixel values
with steady red (64) or predetermined red pulses (255 for 90 ms every 500 ms).
Global brightness remains the normal 100/255, with normal LED correction and
120 Hz writes. Pulses do not depend on the detected beat.

The additional instrumentation affects CPU time; both conditions use the same
instrumentation. It replaces the existing raw-microphone telemetry buffer rather
than allocating a second large buffer. There is no sampling code or extra state
in the normal build. Do not install these environments for normal lamp use.

```sh
uv run --locked pio run -e nanoatmega328_sampling_steady -e nanoatmega328_sampling_pulsed
uv run --locked python experiments/production-sampling/test_capture.py
uv run --locked python experiments/production-sampling/capture.py \
  --port /dev/cu.usbserial-A50285BI --seconds 90 --condition steady \
  --track 'Radioactive — Imagine Dragons' --reference-bpm 136 \
  --output captures/sampling-radioactive-steady \
  --firmware .pio/build/nanoatmega328_sampling_steady/firmware.hex \
  --restore-firmware /path/to/preserved-normal-firmware.hex
```

Preserve the normal firmware outside `.pio/build` before changing PlatformIO
configuration. The recorder additionally copies that image to its output folder
and restores/verifies it in `finally`, including on an interrupted recording.
Run a silent pilot before music. Start playback only after `RECORDER READY`;
pause before the bounded recording finishes, and while flashing or analyzing.
Rewind to the same song position while paused between conditions. Do not change
the playback volume. A 90-second capture leaves room for playback startup,
roughly 75 seconds of music and a short paused tail.

## Recorded data

Each `LS` v1 packet contains one completed detector window. The closing ADC
reading belongs to that window, matching `BeatTracker::sample`. ADC timestamps
are taken after conversion. Inter-sample gap extrema include the gap between
the last sample of the preceding window and the first sample of this window.
This does not observe the waveform during LED serialization or analysis gaps.

All integers are little endian; CRC-8 uses the existing telemetry polynomial 7.

| Byte | Field |
| --- | --- |
| 0–3 | `LS`, version 1, packet size 42 |
| 4–5 | Window sequence (wraps at 65536) |
| 6–9 | Detector window time, milliseconds |
| 10–17 | First/last ADC conversion-end timestamp, microseconds |
| 18–23 | Sample count, minimum/maximum sample gap in microseconds |
| 24–29 | ADC minimum, maximum, detector's actual peak-to-peak |
| 30–33 | Sum of all ADC readings in the window |
| 34–37 | Onset, live BPM, confidence, samples while analysis pending |
| 38–39 | Cumulative firmware packet drops |
| 40 | Last commanded red channel value |
| 41 | CRC |

The bounded buffer retries only when the entire packet fits the UART. It counts
any overwritten windows. The decoder verifies CRC, sequence continuity, ADC
range, and that the measured extrema exactly reproduce the detector input.
Normal `LM` frames and `LT` timing/stack reports are recorded alongside it.
`windows.csv` can be replayed directly through the actual C++ detector.

## Analysis

Store `playback.json` beside each take with `beforePlayUnixMs`,
`afterPlayUnixMs`, `beforePauseUnixMs`, `afterPauseUnixMs`, `startProgressMs`,
`track` and observed `volume`. These bracket UI actions, not acoustic onset.

```sh
uv run --no-project --with numpy==2.5.3 --with matplotlib==3.11.2 \
  python experiments/production-sampling/analyze.py \
  captures/sampling-radioactive-steady captures/sampling-radioactive-pulsed \
  captures/sampling-radioactive-steady-repeat \
  --output docs/measurements/production-sampling
```

The comparison uses host/device timestamps for initial alignment, then finds the
best ±2-second shift of 100 ms smoothed microphone envelopes. It reports that
correlation and shift rather than assuming UI clicks started identical audio
at identical times. Metrics use song seconds 15–70, excluding startup and the
paused tail. A single pair cannot separate all room/playback variation from LED
effects; repeat a condition before interpreting a small difference as causal.

`replay_budget.cpp` also probes sensitivity to 8, 12, 16 or 28 work calls per
window. This is deliberately a coarse experiment: work calls have different
costs, and it does not simulate AVR instruction timing, interrupts or the LED
scheduler. Improved agreement under a budget is evidence of scheduling
sensitivity, not proof that this is the exact hardware schedule.

The committed compressed windows can also be replayed without local captures:

```sh
c++ -std=c++17 -O2 -I lib/LampLogic \
  experiments/production-sampling/replay_budget.cpp -o /tmp/lampe-replay-budget
gzip -dc docs/measurements/production-sampling/pulsed/windows.csv.gz | \
  /tmp/lampe-replay-budget 12
```
