"""Synthetic checks for packaged-launcher profile selection and import dispatch."""

import json
import os
from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from launcher import data_root, profiles, valid_profile
from import_game import CONVERTERS, convert


class LauncherTests(unittest.TestCase):
    def test_only_completed_converted_profiles_are_playable(self):
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            profile = root / "profiles" / "my-game"
            for name in ("settings.json", "import-report.json", "EXTRACTED/ggf/manifest.json",
                         "EXTRACTED/data/combat.json", "EXTRACTED/ui/font.png",
                         "EXTRACTED/ui/charset3.png"):
                path = profile / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"{}")
            (profile / "import-report.json").write_text(json.dumps({"assets_converted": False}))
            self.assertFalse(valid_profile(profile))
            self.assertEqual(profiles(root), [])
            (profile / "import-report.json").write_text(json.dumps({"assets_converted": True}))
            self.assertTrue(valid_profile(profile))
            self.assertEqual(profiles(root), [profile])

    def test_private_data_directory_can_be_overridden(self):
        with TemporaryDirectory() as temporary, patch.dict(os.environ, {"RISE2_USER_DATA": temporary}):
            self.assertEqual(data_root(), Path(temporary).resolve())

    def test_converters_run_in_process_with_explicit_arguments(self):
        calls = []

        class Stopped(Exception):
            pass

        def first_job(arguments):
            calls.append(arguments)
            raise Stopped

        with TemporaryDirectory() as temporary, patch.dict(CONVERTERS, {"extract_ggf.py": first_job}):
            game = Path(temporary) / "game"
            output = Path(temporary) / "output"
            with self.assertRaises(Stopped):
                convert(game, output, log=lambda *_: None)
            self.assertEqual(calls, [["--source", str(game), "--output", str(output / "ggf")]])


if __name__ == "__main__":
    unittest.main()
