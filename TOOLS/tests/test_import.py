"""Synthetic fixtures only: no original game bytes or recordings in this suite."""
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import wave
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pycdlib
from cue_to_iso import convert as cue_to_iso
from disc_image import SectorReader, extract_cue, read_cue, safe_path
from import_game import import_music, required_files, run_import, unzip
from physical_cd import parse_toc, write_audio_tracks


def fake_game(path):
    path.mkdir(parents=True)
    for name in required_files() | {f"MG{x}.{ext}" for x in "ABCDEF" for ext in ("MRS", "MRW")}:
        (path / name).write_bytes(b"synthetic fixture " + name.encode())
    return path


def fake_wav(path, frames=588):
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as out:
        out.setparams((2, 2, 44100, 0, "NONE", "not compressed"))
        out.writeframes(struct.pack("<hh", 123, -123) * frames)


def make_iso(folder, target):
    iso = pycdlib.PyCdlib()
    iso.new(interchange_level=3, vol_ident="SYNTHETIC")
    iso.add_directory("/RISE2")
    for path in sorted(folder.iterdir()):
        iso.add_file(str(path), iso_path="/RISE2/" + path.name + ";1")
    iso.write(str(target))
    iso.close()


def make_cue(iso_path, cue):
    data = iso_path.read_bytes()
    assert len(data) % 2048 == 0
    count = len(data) // 2048
    binary = cue.with_suffix(".bin")
    with binary.open("wb") as out:
        for start in range(0, len(data), 2048):
            out.write(b"\0" + b"\xff" * 10 + b"\0" + b"\0\0\0\1" + data[start:start + 2048] + bytes(288))
        out.write(struct.pack("<hh", 123, -123) * 588 * 7)
    def msf(n): return f"{n // 4500:02}:{n // 75 % 60:02}:{n % 75:02}"
    cue.write_text(f'FILE "{binary.name}" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n'
                   f' TRACK 02 AUDIO\n PREGAP 00:02:00\n INDEX 01 {msf(count)}\n'
                   f' TRACK 03 AUDIO\n INDEX 00 {msf(count + 3)}\n INDEX 01 {msf(count + 5)}\n')
    return binary


class Imports(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.game = fake_game(self.root / "original/RISE2")

    def tearDown(self):
        self.temp.cleanup()

    def run_case(self, source, name="profile", **kwargs):
        return run_import([source], self.root / name, convert_assets=False, log=lambda *_: None, **kwargs)

    def test_folder_auto_digital_no_cd(self):
        report = self.run_case(self.game.parent)
        self.assertEqual(report["music"]["selected"], "digital")
        self.assertEqual(report["music"]["digital_banks"], ["MG" + x for x in "ABCDEF"])
        self.assertEqual((self.root / "profile/game/RBT0.MVS").read_bytes(), (self.game / "RBT0.MVS").read_bytes())

    def test_folder_external_numbered_audio(self):
        music = self.root / "tracks"
        fake_wav(music / "Piste 02.wav")
        fake_wav(music / "03 - Another title.wav")
        report = self.run_case(self.game, music=[music])
        self.assertEqual(report["music"]["selected"], "cd")
        self.assertEqual([t["file"] for t in report["music"]["tracks"]], ["02.wav", "03.wav"])
        self.assertTrue(any("incomplete" in w for w in report["warnings"]))

    def test_zip_and_iso_equal_folder(self):
        baseline = self.run_case(self.game, "folder")
        archive = self.root / "game.zip"
        with zipfile.ZipFile(archive, "w") as out:
            for p in self.game.iterdir():
                out.write(p, "nested/RISE2/" + p.name)
        iso = self.root / "game.iso"
        make_iso(self.game, iso)
        for name, source in (("zip", archive), ("iso", iso)):
            report = self.run_case(source, name)
            self.assertEqual(report["game_files"], baseline["game_files"])

    def test_cue_data_and_audio_with_pregaps(self):
        iso = self.root / "game.iso"
        make_iso(self.game, iso)
        cue = self.root / "game.cue"
        make_cue(iso, cue)
        report = self.run_case(cue)
        self.assertEqual(report["music"]["selected"], "cd")
        for number, sectors in ((2, 3), (3, 2)):
            with wave.open(str(self.root / "profile/music" / f"{number:02}.wav")) as f:
                self.assertEqual(f.getnframes(), sectors * 588)
                self.assertEqual(f.readframes(1), struct.pack("<hh", 123, -123))
        self.assertEqual((self.root / "profile/game/RBT0.MVS").read_bytes(), (self.game / "RBT0.MVS").read_bytes())

    def test_cue_to_iso_matches_original_data_track(self):
        original = self.root / "game.iso"
        make_iso(self.game, original)
        cue = self.root / "game.cue"
        make_cue(original, cue)
        destination = self.root / "converted.iso"
        result = cue_to_iso(cue, destination)
        self.assertEqual(destination.read_bytes(), original.read_bytes())
        self.assertEqual(result["track"], 1)
        with self.assertRaises(FileExistsError):
            cue_to_iso(cue, destination)

    def test_multi_file_cue_and_sector_seek(self):
        iso = self.root / "game.iso"
        make_iso(self.game, iso)
        audio = self.root / "audio.bin"
        audio.write_bytes(bytes(2352 * 2))
        cue = self.root / "game.cue"
        cue.write_text('FILE "game.iso" BINARY\nTRACK 01 MODE1/2048\nINDEX 01 00:00:00\n'
                       'FILE "audio.bin" BINARY\nTRACK 02 AUDIO\nINDEX 01 00:00:00\n')
        tracks = read_cue(cue)
        with SectorReader(tracks[0]) as raw, io.BufferedReader(raw) as reader:
            reader.seek(32767)
            self.assertEqual(reader.read(4098), iso.read_bytes()[32767:32767 + 4098])
        self.run_case(cue)

    def test_music_zip_and_explicit_mode(self):
        wav = self.root / "02.wav"
        fake_wav(wav)
        archive = self.root / "music.zip"
        with zipfile.ZipFile(archive, "w") as out:
            out.write(wav, "music/02.wav")
        report = self.run_case(self.game, music=[archive], music_mode="digital")
        self.assertEqual(report["music"]["selected"], "digital")
        self.assertEqual(len(report["music"]["tracks"]), 1)

    def test_bonus_audio_is_not_used_as_game_soundtrack(self):
        (self.root / "bonus.bin").write_bytes(bytes(2352 * 2))
        cue = self.root / "bonus.cue"
        cue.write_text('FILE "bonus.bin" BINARY\nTRACK 02 AUDIO\nINDEX 01 00:00:00\n')
        report = run_import([self.game, cue], self.root / "profile", convert_assets=False, log=lambda *_: None)
        self.assertEqual(report["music"]["selected"], "digital")
        self.assertEqual(report["music"]["tracks"], [])
        self.assertTrue((self.root / "profile/sources/02/music/02.wav").is_file())

    def test_physical_cd_music_follows_its_game_folder(self):
        def rip(_drive, destination, _log):
            fake_wav(destination / "02.wav")
            return [{"number": 2}]
        with patch("import_game.optical_drive", return_value=True), patch("physical_cd.rip_windows_cd", side_effect=rip):
            report = self.run_case(self.game.parent)
        self.assertEqual(report["sources"][0]["kind"], "physical_cd")
        self.assertEqual(report["music"]["selected"], "cd")
        self.assertEqual(report["music"]["tracks"][0]["number"], 2)

    def test_data_only_virtual_cd_imports_without_audio_toc(self):
        with patch("import_game.optical_drive", return_value=True), patch(
                "physical_cd.rip_windows_cd", side_effect=OSError("TOC absent")):
            report = self.run_case(self.game.parent)
        self.assertEqual(report["sources"][0]["kind"], "physical_cd")
        self.assertEqual(report["music"]["selected"], "digital")
        self.assertTrue(any("virtual/optical drive" in warning for warning in report["warnings"]))

    def test_duplicate_and_bad_numbered_music(self):
        music = self.root / "music"
        fake_wav(music / "01.wav")
        with self.assertRaisesRegex(ValueError, "Ambiguous track number"):
            self.run_case(self.game, music=[music])
        (music / "01.wav").rename(music / "02.wav")
        fake_wav(music / "Piste 02.wav")
        with self.assertRaisesRegex(ValueError, "Two files"):
            self.run_case(self.game, music=[music])
        self.assertFalse((self.root / "profile").exists())

    def test_no_overwrite_and_failed_import_is_atomic(self):
        output = self.root / "profile"
        output.mkdir()
        (output / "precious.txt").write_text("keep")
        with self.assertRaisesRegex(ValueError, "Profile already exists"):
            self.run_case(self.game)
        self.assertEqual((output / "precious.txt").read_text(), "keep")
        (self.game / "VSFACE.ANL").unlink()
        with self.assertRaisesRegex(ValueError, "Required port data missing"):
            self.run_case(self.game, "missing")
        self.assertFalse((self.root / "missing").exists())

    def test_two_versions_not_silently_merged(self):
        other = fake_game(self.root / "original/other")
        (other / "RBT0.MVS").write_bytes(b"different edition")
        with self.assertRaisesRegex(ValueError, "Different game editions"):
            self.run_case(self.game.parent)

    def test_requested_unavailable_cd_fails(self):
        with self.assertRaisesRegex(ValueError, "no available data"):
            self.run_case(self.game, music_mode="cd")

    def test_zip_rejects_traversal_collisions_and_links(self):
        for i, names in enumerate((("../escape",), ("/absolute",), ("C:/absolute",), ("game/X", "GAME/x"))):
            archive = self.root / f"bad{i}.zip"
            with zipfile.ZipFile(archive, "w") as out:
                for name in names:
                    out.writestr(name, b"bad")
            with self.assertRaises(ValueError):
                unzip(archive, self.root / f"unzip{i}")
        archive = self.root / "link.zip"
        with zipfile.ZipFile(archive, "w") as out:
            info = zipfile.ZipInfo("link")
            info.external_attr = 0o120777 << 16
            out.writestr(info, "../elsewhere")
        with self.assertRaises(ValueError):
            unzip(archive, self.root / "unziplink")
        self.assertFalse((self.root / "escape").exists())

    def test_bin_without_cue_and_cue_escape_fail(self):
        binary = self.root / "raw.bin"
        binary.write_bytes(bytes(2352))
        with self.assertRaisesRegex(ValueError, "CUE"):
            self.run_case(binary)
        cue = self.root / "bad.cue"
        cue.write_text('FILE "../outside.bin" BINARY\nTRACK 01 AUDIO\nINDEX 01 00:00:00\n')
        with self.assertRaises(ValueError):
            read_cue(cue)

    def test_reserved_paths_rejected(self):
        for name in ("NUL.txt", "a/CON", "a/../b", "a:b", "a."):
            with self.assertRaises(ValueError):
                safe_path(self.root, name)


class PhysicalCD(unittest.TestCase):
    def toc(self):
        entries = []
        for number, data, sector in ((1, True, 0), (2, False, 5), (3, False, 10), (0xAA, False, 15)):
            lba = sector + 150
            entries.append(bytes((0, 0x14 if data else 0x10, number, 0, 0, lba // 4500, lba // 75 % 60, lba % 75)))
        return struct.pack(">HBB", 34, 1, 3) + b"".join(entries)

    def test_toc_audio_numbers_and_read_lengths(self):
        tracks = parse_toc(self.toc())
        calls = []
        def read(sector, count):
            calls.append((sector, count))
            return bytes(count * 2352)
        with tempfile.TemporaryDirectory() as temp:
            records = write_audio_tracks(tracks, read, temp, lambda *_: None)
            self.assertEqual([r["number"] for r in records], [2, 3])
            self.assertEqual(calls, [(5, 5), (10, 5)])

    def test_truncated_toc_and_sector_read_fail(self):
        with self.assertRaises(ValueError):
            parse_toc(self.toc()[:-1])
        with tempfile.TemporaryDirectory() as temp, self.assertRaises(OSError):
            write_audio_tracks(parse_toc(self.toc()), lambda *_: b"", temp, lambda *_: None)


if __name__ == "__main__":
    unittest.main()
