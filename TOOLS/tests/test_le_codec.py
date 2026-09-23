"""Small LE executables built from scratch; no original executable bytes."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from le_codec import flatten


def executable():
    header = 0x80
    data = bytearray(0x400 + 48)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 60, header)
    data[header:header + 4] = b"LE\0\0"
    values = {0x14: 3, 0x18: 1, 0x1c: 4, 0x28: 16, 0x2c: 16,
              0x40: 0xb0, 0x44: 2, 0x48: 0xe0, 0x68: 0xf0,
              0x6c: 0x110, 0x80: 0x400}
    for offset, value in values.items():
        struct.pack_into("<I", data, header + offset, value)
    for i, obj in enumerate(((32, 0x10000, 5, 1, 2, 0), (16, 0x20000, 3, 3, 1, 0))):
        struct.pack_into("<6I", data, header + 0xb0 + i * 24, *obj)
    data[header + 0xe0:header + 0xec] = b"\0\0\1\0\0\0\2\0\0\0\3\0"
    pages = [struct.pack("<BBhBH", 7, 0, 4, 2, 2) + struct.pack("<BBhBI", 7, 0x10, 14, 2, 4),
             struct.pack("<BBhBI", 7, 0x10, -2, 2, 4) + struct.pack("<BBhB", 2, 0, 4, 2),
             struct.pack("<BBhBH", 8, 0, 0, 1, 8)]
    offsets = [0]
    for records in pages:
        offsets.append(offsets[-1] + len(records))
    struct.pack_into("<4I", data, header + 0xf0, *offsets)
    data[header + 0x110:header + 0x110 + offsets[-1]] = b"".join(pages)
    struct.pack_into("<H", data, 0x400 + 20, 0x1234)
    return data


class LELoader(unittest.TestCase):
    def test_object_pages_last_page_signed_boundary_and_relative_fixups(self):
        image, metadata = flatten(executable())
        self.assertEqual(struct.unpack_from("<I", image, 4)[0], 0x20002)
        self.assertEqual(struct.unpack_from("<I", image, 14)[0], 0x20004)
        self.assertEqual(struct.unpack_from("<I", image, 0x10000)[0], 0xffff0004)
        self.assertEqual(metadata["entry"], 0x10004)
        self.assertEqual(metadata["fixup_records"], 5)
        self.assertEqual(metadata["patched_addresses"], 3)

    def test_selector_has_no_target_offset_and_remains_annotated(self):
        image, metadata = flatten(executable())
        self.assertEqual(struct.unpack_from("<H", image, 20)[0], 0x1234)
        self.assertEqual(metadata["selector_fixups"], [{"address": 0x10014, "target_object": 2}])

    def test_truncated_last_fixup_rejected(self):
        data = executable()
        end = struct.unpack_from("<I", data, 0x80 + 0xf0 + 12)[0]
        struct.pack_into("<I", data, 0x80 + 0xf0 + 12, end - 1)
        with self.assertRaisesRegex(ValueError, "Record de fixup"):
            flatten(data)

    def test_conflicting_cross_page_duplicate_rejected(self):
        data = executable()
        struct.pack_into("<I", data, 0x80 + 0x110 + 16 + 5, 6)
        with self.assertRaisesRegex(ValueError, "contradictoires"):
            flatten(data)

    def test_truncated_data_and_unknown_fixup_rejected(self):
        with self.assertRaisesRegex(ValueError, "tronqué"):
            flatten(executable()[:-1])
        data = executable()
        data[0x80 + 0x110] = 6
        with self.assertRaisesRegex(ValueError, "non pris en charge"):
            flatten(data)


if __name__ == "__main__":
    unittest.main()
