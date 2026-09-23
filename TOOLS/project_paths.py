"""Portable defaults for local, private game data and analysis outputs."""
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORKSPACE = Path(os.environ.get("RISE2_WORKSPACE", ROOT / "LOCAL/default"))
_legacy_source = ROOT / "SRC/DOS_version_install_cracked/RISE2"
SOURCE = Path(os.environ.get("RISE2_SOURCE", WORKSPACE / "game"))
if not SOURCE.exists() and "RISE2_SOURCE" not in os.environ and _legacy_source.exists():
    SOURCE = _legacy_source
EXTRACTED = Path(os.environ.get("RISE2_EXTRACTED", ROOT / "EXTRACTED"))
ANALYSIS = Path(os.environ.get("RISE2_ANALYSIS", ROOT / "ANALYSIS"))
