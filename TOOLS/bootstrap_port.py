"""Download pinned Windows x64 build dependencies from their official projects."""
import argparse
import hashlib
from pathlib import Path
import shutil
import tempfile
from urllib.request import urlopen

from import_game import unzip
from project_paths import ROOT

PACKAGES = (
    ("SDL2-2.30.11", "https://github.com/libsdl-org/SDL/releases/download/release-2.30.11/SDL2-devel-2.30.11-VC.zip",
     "6e524b5f0e3ecd9fe3b439336b5e7c3fe597e8823e8e62c8e3074d335e9ca7d9"),
    ("SDL2_image-2.8.4", "https://github.com/libsdl-org/SDL_image/releases/download/release-2.8.4/SDL2_image-devel-2.8.4-VC.zip",
     "ae2a3e85bee51086184d3f72a47086c61f2acec86984b33f8bb28dab23e5a0d0"),
    ("json.hpp", "https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp",
     "9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6"),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "PORT/thirdparty")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for name, url, expected in PACKAGES:
        target = args.output / name
        if target.exists():
            print(f"Already present : {name}")
            continue
        with tempfile.TemporaryDirectory(prefix=".download-", dir=args.output) as temporary:
            temp = Path(temporary)
            download = temp / "package"
            with urlopen(url, timeout=60) as src, download.open("wb") as dst:
                shutil.copyfileobj(src, dst, 1024 * 1024)
            with download.open("rb") as fp:
                actual = hashlib.file_digest(fp, "sha256").hexdigest()
            if actual != expected:
                raise ValueError(f"SHA-256 mismatch: {name}")
            if name.endswith(".hpp"):
                download.rename(target)
            else:
                unzip(download, temp / "unpacked")
                (temp / "unpacked" / name).rename(target)
        print(f"Installed and verified : {name}")


if __name__ == "__main__":
    main()
