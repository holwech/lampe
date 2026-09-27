# LED → microphone interference measurement

[Measured results from the physical lamp](../../docs/led-microphone-measurements.md).

An isolated, **open-loop** diagnostic for the existing ATmega328P lamp. It drives
known light changes while sampling A0; no beat detector controls the output.
It uses the production strip correction, LED count, global brightness (currently
100/255), and nominal 120 Hz write schedule. The experiment never exceeds normal
full-red output; the white control uses level 64 per channel before correction.

The complete sequence takes about **171 seconds**, with music paused and the room
quiet. Tell the person helping when recording actually ends; they do not need to
wait through flashing, analysis or builds. A 20-second transport rehearsal can
run before arranging the quiet-room session.

| Stage | Duration | Purpose |
| --- | ---: | --- |
| Dark, no writes | 8 s | Baseline without LED serial activity |
| Dark, writes at 120 Hz | 8 s | Transmission without changing brightness |
| Steady red 64, red 255, white 64 | 8 s each | Bias/noise versus constant load/color |
| Steps 0↔64, 64↔128, 0↔255 | 18 s each | Six rises and five same-condition falls, 1.5 s holds |
| Fades 0↔255 over 100 ms, 500 ms, 2 s | 12.8 / 16 / 28 s | Four rises/falls at each rate, 1.5 s holds |
| Full red, 90 ms fading pulse every 500 ms | 12 s | Normal beat-flash shape, without microphone feedback |
| Dark recovery | 8 s | Final baseline |

Fades use discrete 120 Hz brightness steps, as production does. ADC reads target
500 µs spacing but record **actual sample-start timestamps**. LED serialization
and packet encoding introduce gaps, which are retained rather than interpolated
away. No reading is possible during the interrupt-blocking LED write. Event
packets record before/after each important `FastLED.show()`, including the exact
last frame of each fade. These are software timestamps, not measured current or
optical onset times; submillisecond settling cannot be established by this setup.

## Run

Preserve the normal image outside `.pio/build` before changing PlatformIO project
configuration (PlatformIO can clean its build directories). The capture tool
copies that image into its local output directory and restores it in `finally`.
A killed process, unplugged USB or power failure can still prevent restoration.

```sh
uv run --locked pio run -e nanoatmega328
cp .pio/build/nanoatmega328/firmware.hex /tmp/lampe-before-led-mic.hex
uv run --locked pio run --project-dir experiments/led-microphone
uv run --locked python experiments/led-microphone/test_capture.py
# Close the browser serial session. Add --pilot-seconds 20 for a rehearsal.
uv run --locked python experiments/led-microphone/capture.py --port YOUR_PORT --output captures/new-led-mic --firmware experiments/led-microphone/.pio/build/nanoatmega328/firmware.hex --restore-firmware /tmp/lampe-before-led-mic.hex
```

For a recording that must begin immediately on confirmation, upload the diagnostic
first, then pass `--already-installed` to the capture command. It waits for the
firmware's ready packet and sends `G`; the firmware does not start on a timer.

## Analysis

```sh
uv run --no-project --with numpy==2.5.3 --with matplotlib==3.11.2 python experiments/led-microphone/analyze.py captures/new-led-mic --output captures/new-led-mic/analysis
```

The primary unit is an uncalibrated 10-bit ADC count, not volts or sound pressure.
Mean, AC RMS and peak-to-peak values are calculated in 10 ms windows. Settling is
relative to the final stable part of each hold (1.0–1.4 s after the transition):
mean must be within the larger of 3 counts or three robust standard deviations;
RMS must be below the stable median plus the larger of 2 counts or three robust
standard deviations. These thresholds were defined before the full recording.
The first qualifying run must last 100 ms and at least 95% of subsequent windows
must agree. Reported settling has 10 ms resolution; no qualifying run is censored,
not called zero. A changed steady-state bias or noise floor is reported separately.
The target hold must itself be stable: first/last 100 ms medians of that final
400 ms may differ by at most 3 mean counts and 2 RMS counts. Otherwise settling
is censored rather than judged against a still-moving reference.

Keep original ADC/serial recordings locally in ignored `captures/`. Checked-in
summaries, 10 ms windows and event timestamps can preserve the findings without
including ambient audio waveforms. One board/room test can establish interference
correlated with the drive pattern, but cannot distinguish supply, ground, ADC
reference, radiated pickup, or acoustic coupling without further measurements.

Protocol: `LF` v1 is 61 bytes: count (1–12), sequence uint16, base µs uint32,
dropped uint16, twelve uint32 slots, CRC-8/0x07. Each slot packs ADC in bits 0–9,
timestamp offset in 4 µs units in bits 10–23, LED level in bits 24–31. `LE` v1
is 25 bytes: size, stage, cycle, phase, level, RGB mask, global brightness,
before/after µs uint32, total scheduled frames uint32, dropped uint16, CRC.
Phases 0/2 are low/high holds, 1/3 rising/falling, 4/5 the last ramp frames.
Stages 254/255 mean ready/finished. All multibyte integers are little endian.
