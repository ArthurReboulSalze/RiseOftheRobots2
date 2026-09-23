"""Boundary and transparency regression tests for the binary image importers."""

import struct
import unittest
from lzw import LZW
from sprite_codec import decode_frame, SpriteFormatError, indexed_frame, render_frame


def u16(n):
    return struct.pack('<H',n)


def codes9(*values):
    value=sum(code << (9*i) for i,code in enumerate(values))
    return value.to_bytes((len(values)*9+7)//8,'little')


class SpriteTests(unittest.TestCase):
    def test_four_coordinate_modes_and_high_coordinate_bits(self):
        raw=(u16(31)+bytes([44,144,0,255,8]) +
             u16(0x600a)+bytes([100,3,4]) +
             u16(0x8005)+bytes([255,5]) +
             u16(0xb002)+bytes([6,7])+b'\xff\xff')
        f=decode_frame(raw)
        self.assertEqual([(s.x,s.y,s.pixels) for s in f.spans],
                         [(812,400,b'\x00\xff\x08'),(612,403,b'\x03\x04'),
                          (511,403,b'\x05'),(515,403,b'\x06\x07')])
        self.assertEqual(f.bbox,(511,400,815,404))

    def test_transparency_is_not_a_palette_index(self):
        raw=u16(8)+bytes([3,4,0])+u16(0x9001)+b'\xff\xff\xff'
        f=decode_frame(raw)
        indices,mask=indexed_frame(f)
        self.assertEqual(indices.tobytes(),b'\x00\x00\xff')
        self.assertEqual(mask.tobytes(),b'\xff\x00\xff')
        self.assertEqual(render_frame(f,bytes(768)).getchannel('A').tobytes(),mask.tobytes())

    def test_empty_frame(self):
        self.assertIsNone(decode_frame(b'\xff\xff').bbox)

    def test_every_truncation_of_a_realistic_frame_is_rejected(self):
        raw=u16(24)+bytes([20,30,1,2,3])+b'\xff\xff'
        for length in range(len(raw)):
            with self.subTest(length=length),self.assertRaises(SpriteFormatError):
                decode_frame(raw[:length])

    def test_relative_first_and_trailing_data_rejected(self):
        for raw in (u16(0x4004)+bytes([1,2])+b'\xff\xff',b'\xff\xff\x00'):
            with self.assertRaises(SpriteFormatError):
                decode_frame(raw)


class LZWTests(unittest.TestCase):
    def test_clear_and_kwkwk(self):
        self.assertEqual(LZW(codes9(256,65,258,257)).decode(),b'AAA')

    def test_initial_dictionary_entry_matches_game(self):
        self.assertEqual(LZW(codes9(65,258,257)).decode(),b'A\x00A')

    def test_output_limit_and_invalid_future_code(self):
        with self.assertRaises(ValueError):
            LZW(codes9(256,65,258,257)).decode(max_out=2)
        with self.assertRaises(ValueError):
            LZW(codes9(65,300)).decode()

    def test_truncated_stream_rejected(self):
        with self.assertRaises(EOFError):
            LZW(b'\x00').decode()


if __name__=='__main__':
    unittest.main()
