# Recorded microphone regression fixtures

These twelve captures came from the physical lamp on 2026-09-26. Four contain
unidentified music, two contain Stevie Wonder's “Isn't She Lovely,” and one
contains room noise after the music was paused. Four further captures contain
“Radioactive” (assumed Imagine Dragons version), and one contains Pink Floyd's
“Have a Cigar.” The earlier music has a user-supplied approximate 120 BPM reference;
the new tracks use published score references of 136 and 120 BPM respectively.
The last Radioactive capture holds LEDs at a constant red level, while retaining
the normal ADC and LED update schedule, to investigate possible self-interference.
They are the recordings documented in
[BPM measurements](../../../docs/bpm-measurements.md).

Each CSV contains the complete captured sequence of detector input windows:

- `time_ms`: device timestamp rebased to start at 10 ms. All intervals, including
  the missing window in music 2, are preserved.
- `peak`: the original 10-bit ADC peak-to-peak measurement for that window.

The files contain 76,460 windows in about 682 KB of plain text. They omit raw ADC
waveforms, serial traffic, recorded onsets, detected BPM and other derived output.
The detector computes its own onsets and tempo from these inputs. Full original
captures remain local under the Git-ignored `captures/` directory; the manifest
records each source capture name. The extraction is `time_ms = 10 +
((device_ms - first_device_ms) & 0xffffffff)` with `peak` copied unchanged.

Run these fixtures with the regular native suite (also run by firmware CI):

```sh
uv run --locked python scripts/test.py
```

`scripts/test.py` compiles the existing native replay tool once, with undefined-
behavior checks, and passes it to `tests/test_recordings.py`. The Python harness
checks fixture lengths, timing and ADC ranges, then evaluates the detector's
outputs. References and expected results are never passed to the detector.

| Fixture | Recorded duration | Current time near reference | Minimum required |
| --- | ---: | ---: | ---: |
| music-01 | 57.919 s | 99.7% | 60% |
| music-02 | 57.934 s | 90.8% | 85% |
| music-03 | 57.915 s | 99.7% | 60% |
| music-04 | 57.925 s | 99.7% | 30% |
| isnt-she-lovely-01 | 72.977 s | 93.8% | 55% |
| isnt-she-lovely-02 | 72.981 s | 93.8% | 75% |
| room-noise | 27.904 s | No locks | No locks |
| radioactive-01 | 87.937 s | 78.2% | 50% |
| radioactive-02, two gaps | 87.990 s | 22.5% | 50% — expected failure; 20% regression floor |
| have-a-cigar-01 | 72.983 s | 90.9% | 50% |
| radioactive-03, three gaps | 72.927 s | 0.0% | 50% — expected failure |
| radioactive-steady-01, constant LEDs | 42.918 s | 77.6% | 65% |

Music checks exclude the first ten seconds and accept ±4 BPM around the reference,
reflecting its approximate nature. The original target is at most 1% of evaluated
windows locked outside that range. Unlocked windows count against minimum coverage, so disabling
the detector cannot pass. The quiet-room check rejects any lock, including during
startup. Floors leave some room for algorithm changes; they are regression bounds,
not desired accuracy targets or exact output snapshots. The original recordings'
bounds are retained for the six earlier music recordings. Twenty seeded shuffles of
music 1 preserve 50 ms sound bursts but destroy their order. False guesses on these
nonmusical inputs are currently accepted and reported by the test run.

Two acquisition targets and four wrong-tempo targets are individual
`unittest.expectedFailure` tests. The more willing standalone tracker finds music
previously missed, but also reports wrong tempos for 9.6%, 16.6%, and 8.9% of the
three difficult fixtures respectively. Their original 1% targets remain explicit;
interim mandatory regression ceilings are 11%, 18%, and 10%. These are acknowledged
limitations, not a change to reference tempos or ±4 BPM tolerances. A future fix
produces an unexpected success until its marker is removed after review.
The new independent `radioactive-03` check failed: live tracking reported the
wrong tempo for 93.5% of snapshots. Replay, which resets at three captured serial
gaps, reported 0% matching and 38.3% wrong (40% interim ceiling). The original
50% acquisition and 1% wrong-tempo targets are both explicit expected failures;
no minimum acquisition regression floor is claimed for this failed capture.
A green CI run does **not** establish successful detection on every passage.

Music 1–3 informed the original tuning. Music 4 was initially a fresh validation
recording, but is now also a regression fixture, not a future held-out benchmark.
Both “Isn't She Lovely” passages were used during the second round of tuning.
The first “Radioactive” recording was used for experiments. The second passage
and “Have a Cigar” were separately captured checks; candidate changes did not
generalize and were rejected. Full timestamps, including the two diagnostic gaps
in radioactive-02, are preserved. All these captures have now informed development;
new recordings are needed for independent validation. The standalone follow-up is
documented in [its measurement report](../../../docs/bpm-standalone.md).
Generated tests still cover 60–200 BPM, tempo changes and controlled edge cases.
These captures do not establish accuracy for other songs, rooms, microphones or
tempos; future validation needs additional known-tempo recordings and beat labels.
