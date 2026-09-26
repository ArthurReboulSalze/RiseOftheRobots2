"""Synthetic ANI rows and terminal records; no original movie data."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from extract_ani import decode, movie_format


def fixture():
    # The leading command-count bytes intentionally disagree with the runs.
    data=bytearray(struct.pack('<H',5)+bytes((99,4,6,0,4,9)))
    data.extend(bytes(768))
    for color in (12,15):
        data.extend(struct.pack('<HH',1,1)+bytes((0,1,color,color)))
    # An incomplete tail is never read by the original count-minus-two player.
    data.extend(b'\x01')
    return bytes(data)


class Movies(unittest.TestCase):
    def test_width_terminated_rows_and_native_tail_limit(self):
        frames,palette=decode(fixture(),4,4,2,3)
        self.assertEqual(len(frames),3)
        self.assertEqual(frames[0],bytes(4)+bytes((6,)*4+(9,)*4)+bytes(4))
        self.assertEqual(frames[2][4:8],bytes((15,15,6,6)))
        self.assertEqual(palette,bytes(768))
        with self.assertRaisesRegex(ValueError,'truncated'):
            decode(fixture(),4,4,2)

    def test_active_frames_remain_strict(self):
        with self.assertRaises(ValueError):
            decode(fixture()[:780],4,4,2,3)
        with self.assertRaises(ValueError):
            decode(fixture(),3,4,2,3)
        with self.assertRaises(ValueError):
            decode(fixture(),4,4,5,3)

    def test_known_families_and_unknown_geometry(self):
        self.assertEqual(movie_format('RALINK'),(640,400,200))
        self.assertEqual(movie_format('RZVICL'),(320,200,100))
        self.assertEqual(movie_format('LLOGO'),(320,200,199))
        with self.assertRaises(ValueError):
            movie_format('UNKNOWN')


if __name__=='__main__':
    unittest.main()
