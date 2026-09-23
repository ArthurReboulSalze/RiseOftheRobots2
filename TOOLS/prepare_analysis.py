"""Prepare a private LE image and optionally create its own Ghidra project."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

from project_paths import ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--ghidra", type=Path, help="Ghidra installation; requires pyghidra and a compatible JDK")
    parser.add_argument("--java-home", type=Path)
    args = parser.parse_args()
    profile = args.profile.resolve()
    analysis = profile / "ANALYSIS"
    if not (profile / "game/RISE2.EXR").is_file():
        parser.error("This profile does not contain RISE2.EXR.")
    if (analysis / "exr_proj").exists():
        parser.error("A Ghidra project already exists here; use ghidra_session.py to reopen it.")
    env = {**os.environ, "RISE2_SOURCE": str(profile / "game"), "RISE2_ANALYSIS": str(analysis)}
    subprocess.run([sys.executable, str(ROOT / "TOOLS/le2flat.py")], env=env, check=True)
    metadata = json.loads((analysis / "RISE2_le_metadata.json").read_text())
    if metadata["skipped_fixups"] or metadata["invalid_segments"]:
        parser.error("Unresolved LE fixups; inspect the report before importing into Ghidra.")
    if args.ghidra:
        if not args.ghidra.is_dir():
            parser.error("Installation Ghidra introuvable.")
        env["GHIDRA_INSTALL_DIR"] = str(args.ghidra.resolve())
        if args.java_home:
            env["JAVA_HOME"] = str(args.java_home.resolve())
        subprocess.run([sys.executable, str(ROOT / "TOOLS/import_exr_ghidra.py")], env=env, check=True)
    print(f"Local analysis prepared: {analysis}")


if __name__ == "__main__":
    main()
