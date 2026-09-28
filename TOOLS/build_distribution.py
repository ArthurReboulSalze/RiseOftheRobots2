"""Build the private Windows package: one visible Rise2.exe and its runtime.

Run after the C++ Release build, with requirements-build.txt installed. Game
files are never included. The generated bundle stays under ignored LOCAL/.
"""

import argparse
import hashlib
from importlib import metadata
import json
from pathlib import Path
import shutil
import subprocess
import sys
from PIL import Image

from project_paths import ROOT


SDL_PACKAGES = ("SDL2-2.30.11", "SDL2_image-2.8.4", "SDL2_mixer-2.8.0")
DLLS = ("SDL2.dll", "SDL2_image.dll", "SDL2_mixer.dll")


def sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main(argv=None) -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "LOCAL/distribution")
    args = parser.parse_args(argv)
    output = args.output.resolve()
    private_root = (ROOT / "LOCAL").resolve()
    if not output.is_relative_to(private_root) or output == private_root:
        parser.error("The distribution output must be a subfolder of ignored LOCAL/.")
    bundle = output / "Rise2"
    work = private_root / "distribution-build"
    for target in (bundle, work):
        if target.is_symlink() or not target.resolve().is_relative_to(private_root):
            parser.error(f"Refusing a build path outside private LOCAL/: {target}")
    native = ROOT / "PORT/build/Release"
    for name in ("rotr2.exe", *DLLS):
        if not (native / name).is_file():
            parser.error(f"Missing Release build file: {native / name}")
    for name in SDL_PACKAGES:
        if not (ROOT / "PORT/thirdparty" / name / "LICENSE.txt").is_file():
            parser.error(f"Missing SDL license: {name}")

    output.mkdir(parents=True, exist_ok=True)
    work.mkdir(parents=True, exist_ok=True)
    template = ROOT / "TOOLS/templates/image_gallery.html"
    icon_png = ROOT / "assets/ROTR2_icon.png"
    if not icon_png.is_file():
        parser.error(f"Missing project icon: {icon_png}")
    icon_ico = work / "ROTR2_icon.ico"
    with Image.open(icon_png) as source_icon:
        source_icon.save(icon_ico, format="ICO", sizes=[(16, 16), (24, 24), (32, 32),
                                                         (48, 48), (64, 64), (128, 128), (256, 256)])
    command = [sys.executable, "-m", "PyInstaller", "--noconfirm", "--clean",
               "--onedir", "--hide-console", "hide-early", "--name", "Rise2",
               "--icon", str(icon_ico),
               "--paths", str(ROOT / "TOOLS"),
               "--add-data", f"{template}:TOOLS/templates",
               "--add-data", f"{icon_ico}:assets",
               "--distpath", str(output), "--workpath", str(work),
               "--specpath", str(work), str(ROOT / "TOOLS/launcher.py")]
    subprocess.run(command, check=True, cwd=ROOT)
    package = output / "Rise2"
    runtime = package / "runtime"
    runtime.mkdir(exist_ok=True)
    for name in ("rotr2.exe", *DLLS):
        shutil.copy2(native / name, runtime / name)
    shutil.copy2(icon_png, runtime / icon_png.name)
    licenses = runtime / "licenses"
    licenses.mkdir(exist_ok=True)
    for name in SDL_PACKAGES:
        shutil.copy2(ROOT / "PORT/thirdparty" / name / "LICENSE.txt", licenses / f"{name}.txt")
    for name in ("Pillow", "pycdlib", "PyInstaller", "pyinstaller-hooks-contrib",
                 "altgraph", "packaging", "pefile", "pywin32-ctypes"):
        distribution = metadata.distribution(name)
        notices = [entry for entry in distribution.files or ()
                   if entry.name.upper().startswith(("LICENSE", "LICENCE", "COPYING"))
                   and ".dist-info" in str(entry)]
        if not notices:
            parser.error(f"Missing third-party license notice: {name}")
        for entry in notices:
            target = licenses / name / entry.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(distribution.locate_file(entry), target)
    python_license = Path(sys.base_prefix) / "LICENSE.txt"
    if not python_license.is_file():
        parser.error(f"Missing Python license: {python_license}")
    shutil.copy2(python_license, licenses / "Python.txt")
    shutil.copy2(ROOT / "LICENSE", package / "LICENSE.txt")
    (package / "START_HERE.txt").write_text(
        "Rise 2 : Resurrection Port\n\n"
        "Double-click Rise2.exe. On first launch, select your own copy of Rise 2 "
        "(folder, ZIP, ISO, CUE/BIN, or CD). The program converts private assets "
        "under your Windows local application-data folder, then starts the game. "
        "No game data is bundled. Keep this entire folder together.\n",
        encoding="utf-8")
    files = [package / "Rise2.exe", *(runtime / name for name in ("rotr2.exe", *DLLS)),
             runtime / icon_png.name]
    (package / "package-manifest.json").write_text(
        json.dumps({"schema": 1, "files": {str(path.relative_to(package)).replace("\\", "/"): sha256(path)
                                           for path in files}}, indent=2) + "\n",
        encoding="utf-8")
    print(f"Windows package ready: {package}")
    print("Contains no original game assets; distribute the complete folder.")


if __name__ == "__main__":
    main()
