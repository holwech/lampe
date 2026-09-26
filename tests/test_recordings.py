#!/usr/bin/env python3
"""Regression-check real microphone windows against a compiled beat replay tool."""

import csv
import io
import json
from pathlib import Path
import subprocess
import sys
import unittest

FIXTURES = Path(__file__).resolve().parent / "fixtures/microphone"


class RecordedMicrophoneTests(unittest.TestCase):
    def test_recordings(self):
        for case in json.loads((FIXTURES / "manifest.json").read_text()):
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
                recording = "device_ms,peak,onset\n" + "".join(
                    f"{time},{peak},0\n" for time, peak in samples
                )
                result = subprocess.run(
                    [str(REPLAY)], input=recording, text=True,
                    capture_output=True, check=True,
                )
                replay = list(csv.DictReader(io.StringIO(result.stdout)))
                self.assertEqual(len(replay), len(samples))
                self.assertEqual([int(row["device_ms"]) for row in replay],
                                 [time for time, _ in samples])
                bpms = [int(row["bpm"]) for row in replay]

                if "reference_bpm" not in case:
                    # Include startup, too: quiet input must never create a lock.
                    locked = 100 * sum(bpm != 0 for bpm in bpms) / len(bpms)
                    print(f"{case['file']}: {locked:.1f}% locked (room noise)", flush=True)
                    self.assertLessEqual(locked, case["max_locked_percent"])
                    continue

                evaluated = [bpm for (time, _), bpm in zip(samples, bpms)
                             if time - samples[0][0] >= case["warmup_ms"]]
                self.assertTrue(evaluated, "Recording must extend beyond warmup")
                matching = sum(abs(bpm - case["reference_bpm"]) <= case["tolerance_bpm"]
                               for bpm in evaluated)
                wrong = sum(bpm != 0 and abs(bpm - case["reference_bpm"]) > case["tolerance_bpm"]
                            for bpm in evaluated)
                matching_percent = 100 * matching / len(evaluated)
                wrong_percent = 100 * wrong / len(evaluated)
                print(f"{case['file']}: {matching_percent:.1f}% near reference, "
                      f"{wrong_percent:.1f}% wrong tempo", flush=True)
                self.assertGreaterEqual(matching_percent, case["min_matching_percent"],
                                        "Too little time locked near the recorded reference")
                self.assertLessEqual(wrong_percent, case["max_wrong_percent"],
                                     "Too much time locked at a wrong tempo")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: test_recordings.py PATH_TO_COMPILED_BEAT_REPLAY")
    REPLAY = Path(sys.argv[1]).resolve()
    unittest.main(argv=[sys.argv[0]])
