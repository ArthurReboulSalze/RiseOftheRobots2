"""One-entry Windows launcher for private imports and the prebuilt SDL port.

The distributed launcher contains the Python importer but no game data. The
player points it at an owned copy on first use; converted profiles live in the
user's application-data directory and survive updates to the launcher.
"""

import argparse
import json
import multiprocessing
import os
from pathlib import Path
import re
import subprocess
import sys
import tkinter as tk
from tkinter import messagebox, ttk

from import_game import run_import
from import_gui import ImportWindow
from project_paths import ROOT


def install_root() -> Path:
    return Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else ROOT


def game_executable() -> Path:
    if getattr(sys, "frozen", False):
        return install_root() / "runtime" / "rotr2.exe"
    return ROOT / "PORT" / "build" / "Release" / "rotr2.exe"


def data_root() -> Path:
    configured = os.environ.get("RISE2_USER_DATA")
    if configured:
        return Path(configured).expanduser().resolve()
    base = os.environ.get("LOCALAPPDATA")
    return (Path(base) if base else Path.home() / "AppData" / "Local") / "Rise2ResurrectionPort"


def valid_profile(path: Path) -> bool:
    required = ("settings.json", "import-report.json", "EXTRACTED/ggf/manifest.json",
                "EXTRACTED/data/combat.json", "EXTRACTED/ui/font.png",
                "EXTRACTED/ui/charset3.png")
    if not all((path / name).is_file() for name in required):
        return False
    try:
        report = json.loads((path / "import-report.json").read_text(encoding="utf-8"))
        return report.get("assets_converted") is True
    except (OSError, ValueError):
        return False


def profiles(root: Path) -> list[Path]:
    folder = root / "profiles"
    return sorted((p for p in folder.iterdir() if p.is_dir() and valid_profile(p)),
                  key=lambda p: p.name.casefold()) if folder.is_dir() else []


def verify_package() -> list[str]:
    files = [game_executable()]
    if getattr(sys, "frozen", False):
        files += [install_root() / "runtime" / name for name in
                  ("SDL2.dll", "SDL2_image.dll", "SDL2_mixer.dll", "ROTR2_icon.png")]
        files.append(ROOT / "TOOLS" / "templates" / "image_gallery.html")
        files.append(ROOT / "assets" / "ROTR2_icon.ico")
    missing = [str(path) for path in files if not path.is_file()]
    if getattr(sys, "frozen", False) and not missing:
        import hashlib
        manifest = install_root() / "package-manifest.json"
        if not manifest.is_file():
            missing.append(str(manifest))
        else:
            try:
                entries = json.loads(manifest.read_text(encoding="utf-8"))["files"]
                expected_names = {"Rise2.exe", "runtime/rotr2.exe", "runtime/SDL2.dll",
                                  "runtime/SDL2_image.dll", "runtime/SDL2_mixer.dll",
                                  "runtime/ROTR2_icon.png"}
                if set(entries) != expected_names:
                    raise ValueError("Unexpected package manifest entries")
                for name, expected in entries.items():
                    path = install_root() / name
                    if not path.is_file():
                        missing.append(str(path))
                    elif hashlib.sha256(path.read_bytes()).hexdigest() != expected:
                        missing.append(f"Changed file: {path}")
            except (OSError, ValueError, KeyError, TypeError):
                missing.append(f"Invalid package manifest: {manifest}")
    return missing


def launch_game(profile: Path) -> None:
    if not valid_profile(profile):
        raise ValueError(f"The selected profile is incomplete: {profile}")
    missing = verify_package()
    if missing:
        raise FileNotFoundError("Missing packaged runtime: " + ", ".join(missing))
    subprocess.Popen([str(game_executable()), "--assets", str(profile / "EXTRACTED")],
                     cwd=profile, close_fds=True)


def show_launcher(root_dir: Path, smoke=False) -> None:
    root = tk.Tk()
    root.title("Rise 2 : Resurrection Port")
    icon_path = ROOT / "assets" / "ROTR2_icon.ico"
    if icon_path.is_file():
        root.iconbitmap(default=str(icon_path))

    def start_game(profile: Path) -> None:
        try:
            launch_game(profile)
        except (OSError, ValueError) as exc:
            messagebox.showerror("Rise 2", str(exc), parent=root)
            return
        root.destroy()

    def show_import() -> None:
        for child in root.winfo_children():
            child.destroy()
        ImportWindow(root, profile_root=root_dir / "profiles", on_success=start_game)

    available = profiles(root_dir)
    if not available:
        show_import()
    else:
        root.geometry("580x220")
        frame = ttk.Frame(root, padding=20)
        frame.pack(fill="both", expand=True)
        ttk.Label(frame, text="Rise 2 : Resurrection Port", font=("Arial", 18, "bold")).pack(anchor="w")
        ttk.Label(frame, text="Your imported game stays on this computer.").pack(anchor="w", pady=(5, 14))
        chosen = tk.StringVar(value=available[0].name)
        ttk.Combobox(frame, textvariable=chosen, values=[p.name for p in available],
                     state="readonly", width=28).pack(anchor="w")
        row = ttk.Frame(frame)
        row.pack(anchor="w", pady=15)
        ttk.Button(row, text="Play", command=lambda: start_game(root_dir / "profiles" / chosen.get())).pack(side="left")
        ttk.Button(row, text="Import another copy", command=show_import).pack(side="left", padx=10)
    if smoke:
        root.withdraw()
        root.update()
        root.destroy()
    else:
        root.mainloop()


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verify-package", action="store_true", help="Check bundled game and importer files")
    parser.add_argument("--list-profiles", action="store_true", help="List playable local profiles")
    parser.add_argument("--gui-smoke", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--play-profile", help="Start an already imported profile")
    parser.add_argument("--import-source", nargs="+", type=Path, help="Headless import for testing or automation")
    parser.add_argument("--name", default="my-game", help="Profile name for a headless import")
    parser.add_argument("--music", nargs="*", type=Path, default=[])
    parser.add_argument("--music-mode", choices=("auto", "cd", "digital", "effects", "off"), default="auto")
    parser.add_argument("--no-play", action="store_true", help="Do not start the game after headless import")
    parser.add_argument("--import-only", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    if args.verify_package:
        missing = verify_package()
        print("Package complete" if not missing else "Missing: " + ", ".join(missing))
        return bool(missing)
    root_dir = data_root()
    if args.list_profiles:
        print(json.dumps([p.name for p in profiles(root_dir)]))
        return 0
    if args.play_profile:
        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,47}", args.play_profile):
            parser.error("Invalid profile name")
        launch_game(root_dir / "profiles" / args.play_profile)
        return 0
    if args.gui_smoke:
        show_launcher(root_dir, smoke=True)
        print("Launcher window initialized")
        return 0
    if args.import_source:
        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,47}", args.name):
            parser.error("Use a simple profile name (letters, numbers, hyphen or underscore).")
        destination = root_dir / "profiles" / args.name
        run_import(args.import_source, destination, args.music, args.music_mode,
                   convert_assets=not args.import_only)
        if not args.no_play and not args.import_only:
            launch_game(destination)
        return 0
    show_launcher(root_dir)
    return 0


if __name__ == "__main__":
    multiprocessing.freeze_support()
    sys.exit(main())
