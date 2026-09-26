import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import capture


class CaptureTests(unittest.TestCase):
    def test_packet_validation(self):
        packet = bytearray(b"LP\x01\x18\xa0\x0f" + bytes(56))
        for i in range(24):
            packet[14 + 2 * i:16 + 2 * i] = (400 + i).to_bytes(2, "little")
        packet.append(capture.crc8(packet))
        self.assertEqual(capture.decode(packet), (0, 0, list(range(400, 424))))
        for index in range(63):
            for bit in range(8):
                damaged = bytearray(packet)
                damaged[index] ^= 1 << bit
                with self.assertRaises(ValueError):
                    capture.decode(damaged)
        packet[12] = 1
        packet[-1] = capture.crc8(packet[:-1])
        with self.assertRaisesRegex(ValueError, "overflow"):
            capture.decode(packet)

    def test_restore_runs_after_failed_capture_or_flash(self):
        for fail_flash in [False, True]:
            with self.subTest(fail_flash=fail_flash), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                diagnostic, stable = root / "diagnostic.hex", root / "stable.hex"
                diagnostic.write_text("diagnostic")
                stable.write_text("stable")
                args = ["capture.py", "--port", "test-port", "--track", "test", "--output", str(root / "out"),
                        "--firmware", str(diagnostic), "--restore-firmware", str(stable)]
                with patch("sys.argv", args), patch.object(capture, "flash") as flash, \
                     patch.object(capture, "record", side_effect=RuntimeError("capture failed")):
                    if fail_flash:
                        flash.side_effect = [RuntimeError("flash failed"), None]
                    with self.assertRaises(RuntimeError):
                        capture.main()
                    self.assertEqual([c.args[1] for c in flash.call_args_list], [diagnostic, stable])
                metadata = json.loads((root / "out/metadata.json").read_text())
                self.assertFalse(metadata["valid"])
                self.assertTrue(metadata["firmware_restored"])


if __name__ == "__main__":
    unittest.main()
