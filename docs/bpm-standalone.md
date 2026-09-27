# Standalone beat tracker — 2026-09-26

The lamp needs to work without a computer while continuing to refresh its LEDs at
120 Hz. The new integer tracker uses the existing peak-window microphone input,
longer correlation delays, reinforcement from repeated beats, and persistent
competing tempo hypotheses. It was implemented for the AVR's fixed memory budget;
BTrack and its FFT/resampling dependencies remain offline research tools.

False beats on nonmusical room sounds are accepted for this iteration. This does
not make wrong musical tempos correct: both coverage and wrong-tempo rates are
reported below, and the original wrong-tempo targets remain visible in tests.

## Recorded input results

The same saved inputs are replayed before and after the change. References and
±4 BPM tolerances are unchanged, and the initial ten seconds are excluded. These
are approximate tempo comparisons, not independently annotated beat-alignment
measurements. Every recording in this table has informed development; none is a
fresh independent validation set anymore.

| Input | Previous near reference | New near reference | New wrong tempo |
| --- | ---: | ---: | ---: |
| Music 1 | 70.3% | 99.7% | 0.0% |
| Music 2, one gap | 94.5% | 90.8% | 0.0% |
| Music 3 | 95.9% | 99.7% | 0.0% |
| Music 4 | 100.0% | 99.7% | 0.0% |
| Isn't She Lovely 1 | 60.0% | 93.8% | 0.0% |
| Isn't She Lovely 2 | 83.7% | 93.8% | 0.0% |
| Radioactive 1 | 0.0% | 78.2% | 9.6% |
| Radioactive 2, two gaps | 0.0% | 22.5% | 16.6% |
| Have a Cigar | 0.0% | 90.9% | 8.9% |
| Have a Cigar, regular ADC passage 1 | 0.0% | 60.1% | 17.2% |
| Have a Cigar, regular ADC passage 2 | 0.0% | 42.9% | 14.1% |

The regular ADC inputs were recorded with diagnostic firmware and LEDs latched,
then converted to 10 ms peak windows. They are a different sampling condition from
normal operation. The newer paused-music recording produces tempo guesses for
90.1% of evaluated windows; all twenty shuffled controls also produce guesses.
The original quiet-room fixture remains unlocked.

The six earlier music fixtures retain their original passing bounds. The second
Radioactive acquisition target and all three difficult recordings' original 1%
wrong-tempo targets remain explicit expected failures. Interim regression bounds
prevent the newly measured behavior from silently worsening. CI passing is not a
claim that every difficult song has been solved.

Saved summary: [standalone-replay.json](measurements/standalone-replay.json).
The full compressed frequency-feature recordings and upstream comparison remain
in [the research benchmark](../experiments/btrack-benchmark/).

## Scheduling and hardware measurement

A fixed frame clock keeps LED deadlines 8,333 microseconds apart. Due rendering
runs before input polling. Input work needs 750 microseconds of spare time; one
analysis unit needs 1,000. Correlation is split into groups of 32 sample pairs,
and tempo observations into groups of four. A full tempo update may span several
input windows while ADC collection continues. Late analysis is allowed; holding
up an LED deadline is not.

The normal build currently uses 1,461 of 2,048 SRAM bytes. There are no dynamic
allocations in the detector. A separate timing build exercises the real LEDs,
ADC conversion, tracker, and UART with synthetic audio, so timing checks do not
require anyone to keep music playing. It reports frame intervals, worst observed
input/work duration, ongoing-analysis windows, and a stack high-water mark.
The latter measures unused painted SRAM after setup, including interrupt stack use.

```sh
uv run --locked pio run -e nanoatmega328 -e nanoatmega328_timing
# Disconnect the browser's serial session before using the port.
uv run --locked pio run -e nanoatmega328_timing -t upload --upload-port YOUR_PORT
uv run --locked python scripts/measure_timing.py --port YOUR_PORT --seconds 45 --output captures/new-timing
# Always replace the synthetic-input build with normal firmware afterward.
uv run --locked pio run -e nanoatmega328 -t upload --upload-port YOUR_PORT
```

Timing packets (`LT`, version 1, 27 bytes, CRC-8) exist only in the opt-in timing
build. Production sends no extra timing telemetry and uses the real microphone.
The script exercises capture disabled, raw waveform capture, and window capture.
Measured maxima are observations, not a formal worst-case execution-time proof.

## Verification

`uv run --locked python scripts/test.py` covers native arithmetic, a ten-minute
window-counter rollover, incremental/synchronous equivalence on all twelve fixtures,
slow background service, stale history, the full tempo range, beat phase, LED
rendering, program commands and frame-clock rollover. `npm run build` and
`npm test` verify WebAssembly/native parity and serial behavior.

## Measured hardware result

The final synthetic stress run used the exact tracker source deployed afterward.
Across 27 one-second reports, the board maintained 120 LED frames per second.
Observed frame intervals were 8.168–8.484 ms; no interval exceeded the
8.333 ms target by more than the 0.5 ms diagnostic limit. This is a measured
cadence with small loop/interrupt jitter, not a hard real-time timing proof.

- Longest work unit: 552 µs, admitted only with at least 1,000 µs spare.
- Longest ADC/input operation, including synthetic signal generation: 444 µs.
- Minimum untouched stack headroom in the instrumented build: 359 bytes.
- Window capture: 960 consecutive input windows, 0 gaps.
- Tempo analysis crossed at most 2 input-window boundaries per report;
  input sampling continued and the LED clock stayed at 120 Hz.

The normal, real-microphone build was then uploaded and its 14,376 flash bytes
verified by AVRDUDE. It uses 1,461 static SRAM bytes and contains neither the
synthetic input nor timing telemetry. Firmware hashes and measurements are saved
in [deployment](measurements/standalone-deployment.json),
[timing summary](measurements/standalone-timing.json), and
[per-second timing](measurements/standalone-timing.csv).

The earlier timing runs exposed a tight input admission margin, which was raised
from 500 to 750 µs. The final test also distinguishes analysis that spans windows
from actual lost microphone windows; the former is expected, while the latter
fails the check. Exact musical beat alignment still needs labeled beat times.

## Fresh Radioactive check

After deployment, a new 75-second capture supplied 7,160 microphone windows and
2,042 LED telemetry snapshots. Against the same approximate 136 ±4 BPM reference,
the physical lamp did not acquire the song: after ten seconds, it reported
96–108 BPM for 93.5% of snapshots and was unlocked for the remainder. This fresh
check failed, despite the improvements on previously saved inputs.

Both current and previous detectors also miss the reference when replaying this
input. Current replay reports wrong tempos for 38.3% of evaluated windows, versus
38.8% for the previous detector. The serial recording has three gaps (21 rejected
packets, four missing microphone packets); replay resets at those gaps, whereas
the live detector continues on its own complete input. Its lock percentage is
therefore not directly comparable to replay. These telemetry snapshots do not
measure the physical LED refresh rate; that was measured separately above.

The strongest recorded onset repetition gradually changes from roughly 95 to
109 BPM, alongside the lamp's own flashes. Possible feedback from changing LED
current into the microphone needs a controlled comparison; this correlation alone
does not establish its cause. The failed input is retained as `radioactive-03.csv`
with its gaps and original reference intact, with both acquisition and wrong-tempo
targets marked as known failures. No detector tuning has used this fresh capture.
See [the capture summary](measurements/standalone-radioactive-live.json).


A separate 45-second quiet-output comparison used the same detector and ADC/LED
schedule but held all LEDs at red level 64. It captured 4,240 windows with zero
lost/corrupt packets. The physical detector reported 138–140 BPM, near reference
for 71.0% of post-warmup snapshots and wrong for 0%; replay was near reference
77.6% of the time and wrong for 0%. The previous detector never locked on this
input. This is a different passage of the same repeated song, not an identical
waveform under two loads, so it supports but does not prove LED interference.
The input is preserved as `radioactive-steady-01.csv`; summaries and live BPM
traces are in `measurements/standalone-radioactive-{live,steady}*.{json,csv}`.

A subsequent [open-loop LED/microphone experiment](../experiments/led-microphone/)
measures quiet-room steps and fades without allowing microphone readings to
control the LEDs. Normal firmware is restored after every experiment; the
steady-light and synthetic-timing builds are diagnostic tools only.

The completed [quiet-room step/fade experiment](led-microphone-measurements.md)
found no meaningful disturbance at the current brightness: all 57 measured
transitions were within baseline in the first 10 ms bin. It does not support the
earlier feedback hypothesis. Matched music passages and production sampling
timestamps are needed before attributing the BPM failure to the LEDs.
