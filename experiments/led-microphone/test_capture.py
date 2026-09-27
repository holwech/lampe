import unittest
from capture import decode, pcm


class Packets(unittest.TestCase):
    def packet(self, base=123456):
        p = bytearray(61)
        p[:4] = b"LF\x01\x02"
        p[4:6] = (65535).to_bytes(2, "little")
        p[6:10] = base.to_bytes(4, "little")
        for i, (delta, adc, level) in enumerate([(0, 0, 0), (501, 1023, 255)]):
            value = ((delta // 4) << 10) | adc | (level << 24)
            p[12 + 4*i:16 + 4*i] = value.to_bytes(4, "little")
        p[-1] = pcm.crc8(p[:-1])
        return p

    def test_packing_and_timestamp_precision(self):
        d = decode(self.packet())
        self.assertEqual(d["sequence"], 65535)
        self.assertEqual(d["records"], [(123456, 0, 0), (123956, 1023, 255)])

    def test_timestamp_rollover(self):
        self.assertEqual(decode(self.packet(0xffffff00))["records"][1][0], 244)

    def test_corruption(self):
        p = self.packet(); p[20] ^= 1
        with self.assertRaises(ValueError): decode(p)

    def test_event_boundaries(self):
        p = bytearray(25); p[:10] = bytes([76, 69, 1, 25, 7, 5, 2, 255, 1, 100])
        p[10:14] = (3000).to_bytes(4, "little")
        p[14:18] = (3540).to_bytes(4, "little")
        p[-1] = pcm.crc8(p[:-1])
        d = decode(p)
        self.assertEqual((d["stage"], d["cycle"], d["phase"], d["brightness"]), (7, 5, 2, 100))
        self.assertEqual(d["after_us"]-d["before_us"], 540)


if __name__ == "__main__": unittest.main()
