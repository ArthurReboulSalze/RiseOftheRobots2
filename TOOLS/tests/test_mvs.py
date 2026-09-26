"""Synthetic MVS/STS metadata fixtures; no game data required."""
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from extract_mvs import parse_bank, parse_commands, parse_effect_script


class StateMetadataTests(unittest.TestCase):
    def test_command_order_wildcard_and_bounds(self):
        raw = bytes(12) + bytes([1,2,254,2,255,80,255])
        self.assertEqual(parse_commands(raw,12,len(raw),96),
                         [dict(inputs=[1,2,254,2],target=80)])
        for tail in [bytes([1]),bytes([1,255]),bytes([1,255,96]),bytes([1]*17)]:
            with self.subTest(tail=tail), self.assertRaises(ValueError):
                parse_commands(bytes(12)+tail,12,12+len(tail),96)

    def test_effect_end_loop_endianness_and_truncation(self):
        for endian in ['<','>']:
            with self.subTest(endian=endian):
                # A short two-byte stop is used by real banks between pointers.
                raw = struct.pack(endian+'4h',1000,5,-3,2)+struct.pack(endian+'h',-999)
                result = parse_effect_script(raw,0,len(raw),endian=='>')
                self.assertEqual(result[0],dict(image=1000,dx=5,dy=-3,flags=2))
                self.assertEqual(result[1]['image'],-999)
                loop = struct.pack(endian+'8h',3,1,0,3,-1,0,0,0)
                self.assertEqual(parse_effect_script(loop,0,len(loop),endian=='>')[-1]['image'],-1)
                with self.assertRaises(ValueError):
                    parse_effect_script(loop[8:],0,8,endian=='>')
                with self.assertRaises(ValueError):
                    parse_effect_script(raw[:7],0,7,endian=='>')
                with self.assertRaisesRegex(ValueError,'Unterminated'):
                    parse_effect_script(raw[:8],0,8,endian=='>')

    def test_sts_bytes_and_signed_action(self):
        for endian, magic in [("<", b"MVS\x01"), (">", b"\x01SVM")]:
            with self.subTest(endian=endian), tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp) / "SYNTH.MVS"
                bank = bytearray(54)
                bank[:4] = magic
                struct.pack_into(endian + "4I", bank, 4, 54, 54, 20, 0xffffffff)
                struct.pack_into(endian + "7I", bank, 20, 52, 52, 52, 0, 0, 0, 0)
                bank[48:52] = bytes([0x40, 0, 0, 237])
                bank[52:54] = b"\xff\x00"
                path.write_bytes(bank)
                sts = bytearray(96 * 14)
                sts[:7] = bytes([1, 255, 0, 0, 248, 0, 0x10])
                path.with_suffix(".STS").write_bytes(sts)
                move = parse_bank(path)["moves"][0]
                self.assertEqual(move["state"], dict(ground_mode=1, action_type=-1,
                                                     gravity=248, flags=0x10))
                self.assertEqual(move["param"], 237)
                path.with_suffix(".STS").write_bytes(b"truncated")
                with self.assertRaisesRegex(ValueError, "Invalid STS size"):
                    parse_bank(path)


if __name__ == "__main__":
    unittest.main()
