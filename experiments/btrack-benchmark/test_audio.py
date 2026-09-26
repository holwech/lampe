"""Validate the causal audio front ends before interpreting recording results."""
import csv
import io
from pathlib import Path
import subprocess
import sys
import unittest

import numpy as np
from compare_audio import features


class AudioFeaturesTests(unittest.TestCase):
    def test_bands_distinguish_amplitude_modulated_carriers(self):
        time = np.arange(16000) / 4000
        for name, frequency in [("bass", 100), ("mid", 400), ("high", 1200)]:
            adc = np.rint(512 + (80 + 60 * np.sin(2 * np.pi * 2 * time)) * np.sin(2 * np.pi * frequency * time)).astype(int)
            _, onset = features(adc)
            scores = {band: float(np.sum(onset[band][100:])) for band in ["bass", "mid", "high"]}
            self.assertEqual(max(scores, key=scores.get), name)

    def test_future_audio_does_not_change_previous_features(self):
        adc = np.random.default_rng(7).integers(350, 670, size=16000)
        peaks, full = features(adc)
        short_peaks, prefix = features(adc[:8000])
        np.testing.assert_array_equal(peaks[:200], short_peaks)
        for name in full:
            np.testing.assert_array_equal(full[name][:200], prefix[name])
        def upstream(values):
            data = "sample_index,adc\n" + "".join(f"{i},{v}\n" for i, v in enumerate(values))
            result = subprocess.check_output([str(BUILD / "audio_features")], input=data, text=True)
            return list(csv.DictReader(io.StringIO(result)))
        full_complex, short_complex = upstream(adc), upstream(adc[:8000])
        self.assertEqual(full_complex[:200], short_complex)
        self.assertEqual([int(r["peak"]) for r in full_complex], list(peaks))
        self.assertEqual([int(r["time_ms"]) for r in full_complex], list(range(10, 4001, 10)))


if __name__ == "__main__":
    BUILD = Path(sys.argv[1]).resolve()
    unittest.main(argv=[sys.argv[0]])
