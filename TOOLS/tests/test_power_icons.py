"""Synthetic contract for the verified 18-entry executable power table."""

import sys
from pathlib import Path
import struct
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from extract_power_icons import parse_icons, SLOTS, TABLE_ADDRESS


class PowerIconTests(unittest.TestCase):
    def test_table_is_indexed_by_robot_slot(self):
        base = TABLE_ADDRESS - 20
        image = bytearray(20 + 2 * len(SLOTS))
        indices = [i % 6 for i in range(len(SLOTS))]
        struct.pack_into(f"<{len(SLOTS)}H", image, 20, *indices)
        self.assertEqual(parse_icons(bytes(image), base), dict(zip(SLOTS, indices)))

    def test_rejects_invalid_or_short_table(self):
        base = TABLE_ADDRESS
        with self.assertRaises(ValueError):
            parse_icons(bytes(2 * len(SLOTS) - 1), base)
        image = bytearray(2 * len(SLOTS))
        struct.pack_into("<H", image, 0, 6)
        with self.assertRaises(ValueError):
            parse_icons(bytes(image), base)


if __name__ == "__main__":
    unittest.main()
