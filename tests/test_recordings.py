#!/usr/bin/env python3
"""Regression-check real microphone windows against a compiled beat replay tool."""

import csv
import io
import json
from pathlib import Path
import random
import subprocess
import sys
import unittest

FIXTURES = Path(__file__).resolve().parent / "fixtures/microphone"
CASES = json.loads((FIXTURES / "manifest.json").read_text())


class RecordedMicrophoneTests(unittest.TestCase):
    def replay(self, samples):
        # No reference tempo or recorded onset is provided to the detector.
        recording = "device_ms,peak,onset\n" + "".join(
            f"{time},{peak},0\n" for time, peak in samples
        )
        result = subprocess.run(
            [str(REPLAY)], input=recording, text=True, capture_output=True, check=True,
        )
        deferred = subprocess.run(
            [str(REPLAY), "--deferred"], input=recording, text=True,
            capture_output=True, check=True,
        )
        self.assertEqual(result.stdout, deferred.stdout,
                         "Incremental firmware work must match synchronous replay")
        replay = list(csv.DictReader(io.StringIO(result.stdout)))
        self.assertEqual(len(replay), len(samples))
        self.assertEqual([int(row["device_ms"]) for row in replay],
                         [time for time, _ in samples])
        return [int(row["bpm"]) for row in replay]

    def tempo_coverage(self, case, samples, bpms):
        evaluated = [bpm for (time, _), bpm in zip(samples, bpms)
                     if time - samples[0][0] >= case["warmup_ms"]]
        self.assertTrue(evaluated, "Recording must extend beyond warmup")
        matching = sum(abs(bpm - case["reference_bpm"]) <= case["tolerance_bpm"]
                       for bpm in evaluated)
        wrong = sum(bpm != 0 and abs(bpm - case["reference_bpm"]) > case["tolerance_bpm"]
                    for bpm in evaluated)
        return 100 * matching / len(evaluated), 100 * wrong / len(evaluated)

    def test_shuffled_sound(self):
        # Preserve short real sound bursts, but destroy their rhythmic order.
        # Noise locks are explicitly accepted for now. Keep these inputs as
        # arithmetic stress cases and report the behavior rather than hiding it.
        with (FIXTURES / "music-01.csv").open() as file:
            peaks = [int(row["peak"]) for row in csv.DictReader(file)]
        locked_cases = 0
        for seed in range(20):
            with self.subTest(seed=seed):
                blocks = [peaks[i:i + 5] for i in range(0, len(peaks), 5)]
                random.Random(seed).shuffle(blocks)
                shuffled = [peak for block in blocks for peak in block]
                samples = [(10 * (i + 1), peak) for i, peak in enumerate(shuffled)]
                bpms = self.replay(samples)
                self.assertTrue(all(bpm == 0 or 60 <= bpm <= 200 for bpm in bpms))
                locked_cases += any(bpms)
        print(f"Shuffled controls: {locked_cases}/20 produce tempo guesses (accepted noise behavior)", flush=True)

    def test_recordings(self):
        for case in CASES:
            with self.subTest(recording=case["file"]):
                with (FIXTURES / case["file"]).open() as file:
                    reader = csv.DictReader(file)
                    self.assertEqual(reader.fieldnames, ["time_ms", "peak"])
                    samples = [(int(row["time_ms"]), int(row["peak"])) for row in reader]
                self.assertEqual(len(samples), case["windows"])
                self.assertEqual(samples[0][0], 10)
                self.assertEqual(samples[-1][0] - samples[0][0], case["duration_ms"])
                intervals = [b[0] - a[0] for a, b in zip(samples, samples[1:])]
                self.assertTrue(all(delta > 0 for delta in intervals))
                self.assertEqual(sum(delta > 15 for delta in intervals), case["gaps"])
                self.assertTrue(all(0 <= peak <= 1023 for _, peak in samples))

                # The existing replay tool accepts the original diagnostic CSV
                # shape. Recorded onsets are deliberately omitted from fixtures:
                # the detector must derive them from peaks itself. Neither the
                # reference BPM nor the expected results reach the C++ process.
                bpms = self.replay(samples)

                if "reference_bpm" not in case:
                    # Include startup, too: quiet input must never create a lock.
                    locked = 100 * sum(bpm != 0 for bpm in bpms) / len(bpms)
                    print(f"{case['file']}: {locked:.1f}% locked (room noise)", flush=True)
                    self.assertLessEqual(locked, case["max_locked_percent"])
                    continue

                matching_percent, wrong_percent = self.tempo_coverage(case, samples, bpms)
                known_failure = case.get("known_acquisition_failure", False)
                print(f"{case['file']}: {matching_percent:.1f}% near reference, "
                      f"{wrong_percent:.1f}% wrong tempo"
                      + (" (known acquisition failure)" if known_failure else ""), flush=True)
                if not known_failure:
                    self.assertGreaterEqual(matching_percent, case["min_matching_percent"],
                                            "Too little time locked near the recorded reference")
                else:
                    self.assertGreaterEqual(matching_percent, case.get("regression_min_matching_percent", 0))
                self.assertLessEqual(wrong_percent, case.get("regression_max_wrong_percent", case["max_wrong_percent"]),
                                     "Too much time locked at a wrong tempo")


def acquisition_test(case):
    @unittest.expectedFailure
    def test(self):
        with (FIXTURES / case["file"]).open() as file:
            samples = [(int(row["time_ms"]), int(row["peak"])) for row in csv.DictReader(file)]
        matching, _ = self.tempo_coverage(case, samples, self.replay(samples))
        self.assertGreaterEqual(matching, case["min_matching_percent"])
    return test


def wrong_tempo_test(case):
    @unittest.expectedFailure
    def test(self):
        with (FIXTURES / case["file"]).open() as file:
            samples = [(int(row["time_ms"]), int(row["peak"])) for row in csv.DictReader(file)]
        _, wrong = self.tempo_coverage(case, samples, self.replay(samples))
        self.assertLessEqual(wrong, case["max_wrong_percent"])
    return test


# Keep unresolved acquisition targets visible as individual expected failures.
# Fixture integrity and recorded regression bounds remain mandatory. Original
# 1% wrong-tempo targets stay visible when the more willing tracker misses them.
# When a target starts passing, unittest reports an unexpected success: remove
# that case's known-failure marker only after reviewing the measured improvement.
for case in CASES:
    if case.get("known_acquisition_failure", False):
        name = "test_acquisition_" + Path(case["file"]).stem.replace("-", "_")
        setattr(RecordedMicrophoneTests, name, acquisition_test(case))
    if case.get("known_wrong_tempo_failure", False):
        name = "test_wrong_tempo_" + Path(case["file"]).stem.replace("-", "_")
        setattr(RecordedMicrophoneTests, name, wrong_tempo_test(case))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: test_recordings.py PATH_TO_COMPILED_BEAT_REPLAY")
    REPLAY = Path(sys.argv[1]).resolve()
    unittest.main(argv=[sys.argv[0]])
