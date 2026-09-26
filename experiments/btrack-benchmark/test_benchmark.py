#!/usr/bin/env python3
"""Check clock conversion, causal processing and firmware parity of the benchmark."""
import csv
import io
import json
from pathlib import Path
import subprocess
import sys
import unittest

from compare import FIXTURES, replay, summarize


class BenchmarkTests(unittest.TestCase):
    def test_lamp_parity_on_every_recording(self):
        for case in json.loads((FIXTURES / "manifest.json").read_text()):
            with self.subTest(recording=case["file"]):
                with (FIXTURES / case["file"]).open() as file:
                    samples = [(int(r["time_ms"]), int(r["peak"])) for r in csv.DictReader(file)]
                _, compared = replay(BUILD / "beat_benchmark", samples, "default")
                data = "device_ms,peak,onset\n" + "".join(f"{t},{p},0\n" for t, p in samples)
                result = subprocess.run([str(BUILD / "lamp_replay")], input=data, text=True, capture_output=True, check=True)
                native = list(csv.DictReader(io.StringIO(result.stdout)))
                self.assertEqual(len(compared), len(native))
                for a, b in zip(compared, native):
                    self.assertEqual((a["lamp_bpm"], a["onset"], a["lamp_pulse"]),
                                     tuple(int(b[k]) for k in ["bpm", "onset", "pulse"]))

    def test_fixed_rate_and_phase(self):
        for tempo in [100, 136]:
            period = 60000 / tempo
            samples = [(t, 180 if t >= 1000 and (t - 1000) % period < 20 else 4)
                       for t in range(10, 40001, 10)]
            _, rows = replay(BUILD / "beat_benchmark", samples, "default")
            self.assertGreater(summarize(rows, tempo)["btrack"]["near_reference_percent"], 98)
            beats = [r["time_ms"] for r in rows if r["time_ms"] >= 10000 and r["btrack_beat"]]
            self.assertLess(abs(len(beats) - 30000 / period), 2)
            errors = sorted(abs((t - 1000 + period / 2) % period - period / 2) for t in beats)
            self.assertLess(errors[len(errors) // 2], 30)

    def test_jitter_and_gaps_preserve_clock(self):
        _, rows = replay(BUILD / "beat_benchmark", [(10, 40), (21, 90), (33, 40), (200, 20), (211, 40)], "default")
        self.assertEqual([r["btrack_ticks"] for r in rows], [1, 2, 3, 1, 2])
        self.assertEqual([r["segment"] for r in rows], [1, 1, 1, 2, 2])
        # At t=20 the reading from t=21 was not yet available.
        self.assertNotEqual(rows[1]["source_onset"], rows[0]["source_onset"])

    def test_future_samples_cannot_change_previous_output(self):
        samples = [(10 * (i + 1), (i * 31) % 128) for i in range(2000)]
        _, full = replay(BUILD / "beat_benchmark", samples, "default")
        _, prefix = replay(BUILD / "beat_benchmark", samples[:1000], "default")
        self.assertEqual(full[:1000], prefix)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: test_benchmark.py BUILD_DIRECTORY")
    BUILD = Path(sys.argv[1]).resolve()
    unittest.main(argv=[sys.argv[0]])
