"""Synthetic MVS/STS metadata fixtures; no game data required."""
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from extract_mvs import parse_bank


class StateMetadataTests(unittest.TestCase):
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
