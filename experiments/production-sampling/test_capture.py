import unittest

from capture import crc8, decode


def packet():
    data = bytearray(b"LS\x01\x2a" + bytes(38))
    # A window spanning micros() rollover, with two samples 32 us apart.
    values = [(4, 2, 65535), (6, 4, 100), (10, 4, 0xfffffff0), (14, 4, 16),
              (18, 2, 2), (20, 2, 32), (22, 2, 32), (24, 2, 500),
              (26, 2, 520), (28, 2, 20), (30, 4, 1020), (35, 1, 136)]
    for at, size, value in values:
        data[at:at + size] = value.to_bytes(size, "little")
    data[-1] = crc8(data[:-1])
    return data


class DecoderTests(unittest.TestCase):
    def test_window_and_rollover(self):
        row = decode(packet())
        self.assertEqual((row["last_us"] - row["first_us"]) & 0xffffffff, 32)
        self.assertEqual((row["sequence"], row["count"], row["peak"], row["bpm"]), (65535, 2, 20, 136))

    def test_corrupt_or_truncated_packet(self):
        data = packet()
        with self.assertRaises(ValueError):
            decode(data[:-1])
        data[20] ^= 1
        with self.assertRaises(ValueError):
            decode(data)

    def test_disagrees_with_detector_window(self):
        data = packet()
        data[28] = 21
        data[-1] = crc8(data[:-1])
        with self.assertRaises(ValueError):
            decode(data)

    def test_invalid_adc_sum(self):
        data = packet()
        data[30:34] = (2000).to_bytes(4, "little")
        data[-1] = crc8(data[:-1])
        with self.assertRaises(ValueError):
            decode(data)


if __name__ == "__main__":
    unittest.main()
