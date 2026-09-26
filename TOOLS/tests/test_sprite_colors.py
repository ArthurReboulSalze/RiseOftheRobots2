import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from sprite_codec import Frame, Span, render_frame
from extract_anr import palette_for


class SpriteColorTests(unittest.TestCase):
    def test_embedded_fx_override_green_reserves_without_recoloring_robot(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            robot = bytes([1, 2, 3]) * 80
            (source / 'RA.PAL').write_bytes(robot)
            (source / 'AGA.GGF').write_bytes(b'\x01' + bytes([0, 63, 0]) * 256)
            shared = bytearray(bytes([0, 255, 0]) * 256)
            shared[203*3:204*3] = bytes([255, 84, 0])
            (source / 'EXTRA.PAL').write_bytes(bytes(8) + shared)
            palette, label = palette_for(source, 'RBTA', 0)
            self.assertEqual(palette[203*3:204*3], bytes([255, 84, 0]))
            self.assertEqual(palette[0:3], palette[70*3:71*3])
            self.assertNotEqual(palette[0:3], bytes([0, 255, 0]))
            self.assertEqual(palette[140*3:141*3], bytes([0, 255, 0]))
            self.assertIn('EXTRA.PAL', label)

    def test_span_opacity_and_unpainted_rgb_remain_distinct(self):
        palette = bytes([0, 255, 0]) * 256
        frame = Frame(0, 2, (Span(0, 0, b'\x00'), Span(2, 0, b'\x01')))
        image = render_frame(frame, palette)
        self.assertEqual(image.getpixel((0, 0)), (0, 255, 0, 255))
        self.assertEqual(image.getpixel((1, 0)), (0, 0, 0, 0))
        self.assertEqual(image.getpixel((2, 0)), (0, 255, 0, 255))


if __name__ == '__main__':
    unittest.main()
