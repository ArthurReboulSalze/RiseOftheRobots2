"""Build a compact ASCII bitmap atlas for the provisional SDL menu text."""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "EXTRACTED/ui/font.png"
CELL_W, CELL_H, COLUMNS = 16, 20, 16
FIRST, LAST = 32, 126


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=OUTPUT)
    args = parser.parse_args()
    # Pillow's bundled font avoids distributing or requiring a Windows font.
    font = ImageFont.load_default(size=18)
    rows = (LAST - FIRST + COLUMNS) // COLUMNS
    image = Image.new("RGBA", (COLUMNS * CELL_W, rows * CELL_H), (255, 255, 255, 0))
    draw = ImageDraw.Draw(image)
    for code in range(FIRST, LAST + 1):
        glyph = chr(code)
        left, top, right, bottom = draw.textbbox((0, 0), glyph, font=font)
        column = (code - FIRST) % COLUMNS
        row = (code - FIRST) // COLUMNS
        x = column * CELL_W + (CELL_W - (right - left)) // 2 - left
        y = row * CELL_H + (CELL_H - (bottom - top)) // 2 - top
        draw.text((x, y), glyph, font=font, fill=(255, 255, 255, 255))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.output)
    print(args.output)


if __name__ == "__main__":
    main()
