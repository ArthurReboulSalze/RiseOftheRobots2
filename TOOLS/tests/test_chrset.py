"""Synthetic checks for the original fixed-cell CHRSET decoder."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from extract_chrset import decode, read_palette


class CharsetTests(unittest.TestCase):
    def test_ascii_cell_geometry_and_shades(self):
        for size in (6, 12, 24):
            data = bytearray(96 * size * size)
            offset = (ord("A") - 32) * size * size
            data[offset:offset + 5] = bytes((251, 252, 253, 254, 255))
            image = decode(bytes(data), size)
            self.assertEqual(image.size, (16 * size, 6 * size))
            x, y = (ord("A") - 32) % 16 * size, (ord("A") - 32) // 16 * size
            self.assertEqual([image.getpixel((x + i, y)) for i in range(5)],
                             [(220, 220, 220, 255), (192, 192, 192, 255),
                              (172, 172, 172, 255), (0, 0, 0, 255),
                              (255, 255, 255, 255)])
            self.assertEqual(image.getpixel((0, 0))[3], 0)

    def test_rejects_truncated_bank(self):
        with self.assertRaises(ValueError):
            decode(bytes(96 * 12 * 12 - 1), 12)

    def test_extra_palette_header(self):
        from tempfile import TemporaryDirectory
        with TemporaryDirectory() as temporary:
            path = Path(temporary) / "EXTRA.PAL"
            palette = bytes(i % 256 for i in range(768))
            path.write_bytes(bytes(8) + palette)
            self.assertEqual(read_palette(path), palette)


if __name__ == "__main__":
    unittest.main()
