"""Decode the owner's CHRSET1/2/3.DAT into transparent original glyph atlases.

Each bank contains 96 ASCII cells (space through DEL), stored as uncompressed
palette indices. Their dimensions are 6x6, 12x12 and 24x24 respectively.
The five monochrome text shades are remapped for SDL tinting. Coloured
power-symbol cells use the source EXTRA.PAL RGB8 entries when available.
"""

import argparse
from pathlib import Path

from PIL import Image

from project_paths import SOURCE


SIZES = (6, 12, 24)
# The DOS renderer remaps the grey ramp for blue/orange text. Keep its five
# levels distinct while brightening the middle shades before SDL colour mod.
SHADES = {251: 220, 252: 192, 253: 172, 254: 0, 255: 255}


def read_palette(path: Path) -> bytes:
    raw = path.read_bytes()
    if len(raw) == 776:  # EXTRA.PAL has an eight-byte header.
        return raw[8:]
    if len(raw) == 768:
        return raw
    raise ValueError(f"Unexpected palette size in {path}: {len(raw)}")


def decode(data: bytes, size: int, palette: bytes | None = None) -> Image.Image:
    if len(data) != 96 * size * size:
        raise ValueError(f"CHRSET{size} must contain 96 {size}x{size} cells")
    image = Image.new("RGBA", (16 * size, 6 * size))
    pixels = image.load()
    for glyph in range(96):
        ox, oy = glyph % 16 * size, glyph // 16 * size
        for y in range(size):
            for x in range(size):
                index = data[glyph * size * size + y * size + x]
                if index in SHADES:
                    grey = SHADES[index]
                    pixels[ox + x, oy + y] = grey, grey, grey, 255
                elif index and palette and len(palette) == 768:
                    pixels[ox + x, oy + y] = (*palette[index * 3:index * 3 + 3], 255)
    return image


def main(argv=None) -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=SOURCE)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--palette", type=Path)
    args = parser.parse_args(argv)
    palette = read_palette(args.palette) if args.palette and args.palette.exists() else None
    args.output.mkdir(parents=True, exist_ok=True)
    for number, size in enumerate(SIZES, 1):
        source = args.source / f"CHRSET{number}.DAT"
        image = decode(source.read_bytes(), size, palette)
        target = args.output / f"charset{number}.png"
        image.save(target)
        print(f"{source.name}: {size}x{size} cells -> {target}")


if __name__ == "__main__":
    main()
