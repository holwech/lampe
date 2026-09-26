# Recorded microphone regression fixtures

These ten captures came from the physical lamp on 2026-09-26. Four contain
unidentified music, two contain Stevie Wonder's “Isn't She Lovely,” and one
contains room noise after the music was paused. Two further captures contain
“Radioactive” (assumed Imagine Dragons version), and one contains Pink Floyd's
“Have a Cigar.” The earlier music has a user-supplied approximate 120 BPM reference;
the new tracks use published score references of 136 and 120 BPM respectively.
They are the recordings documented in
[BPM measurements](../../../docs/bpm-measurements.md).

Each CSV contains the complete captured sequence of detector input windows:

- `time_ms`: device timestamp rebased to start at 10 ms. All intervals, including
  the missing window in music 2, are preserved.
- `peak`: the original 10-bit ADC peak-to-peak measurement for that window.

The files contain 65,060 windows in about 580 KB of plain text. They omit raw ADC
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
| music-01 | 57.919 s | 70.3% | 60% |
| music-02 | 57.934 s | 94.5% | 85% |
| music-03 | 57.915 s | 95.9% | 60% |
| music-04 | 57.925 s | 100.0% | 30% |
| isnt-she-lovely-01 | 72.977 s | 60.0% | 55% |
| isnt-she-lovely-02 | 72.981 s | 83.7% | 75% |
| room-noise | 27.904 s | No locks | No locks |
| radioactive-01 | 87.937 s | 0.0% | 50% — expected failure |
| radioactive-02, two gaps | 87.990 s | 0.0% | 50% — expected failure |
| have-a-cigar-01 | 72.983 s | 0.0% | 50% — expected failure |

Music checks exclude the first ten seconds and accept ±4 BPM around the reference,
reflecting its approximate nature. They allow at most 1% of evaluated windows locked outside
that range. Unlocked windows count against the minimum coverage, so disabling
the detector cannot pass. The quiet-room check rejects any lock, including during
startup. Floors leave some room for algorithm changes; they are regression bounds,
not desired accuracy targets or exact output snapshots. The original recordings'
bounds were retained when adding the new song. Twenty seeded shuffles of music 1
preserve 50 ms sound bursts but destroy their order; these must never lock.

The three new acquisition targets are individual `unittest.expectedFailure`
tests, selected by `known_acquisition_failure` in the manifest. The installed
detector never acquires a tempo on these inputs; the 50% target is an explicit
unmet improvement goal, not a measured result. Their integrity and wrong-tempo
checks remain mandatory in `test_recordings`. A future fix produces an unexpected
success until its known-failure marker is removed after review. No threshold for
any previously passing recording was reduced. A green CI run with these expected
failures does **not** establish successful detection on the new songs.

Music 1–3 informed the original tuning. Music 4 was initially a fresh validation
recording, but is now also a regression fixture, not a future held-out benchmark.
Both “Isn't She Lovely” passages were used during the second round of tuning.
The first “Radioactive” recording was used for experiments. The second passage
and “Have a Cigar” were separately captured checks; candidate changes did not
generalize and were rejected. Full timestamps, including the two diagnostic gaps
in radioactive-02, are preserved. The gaps are a limitation of that replay, but
the other two new captures have no gaps and also never lock.
Generated tests still cover 60–200 BPM, tempo changes and controlled edge cases.
These captures do not establish accuracy for other songs, rooms, microphones or
tempos; future validation needs additional known-tempo recordings and beat labels.
