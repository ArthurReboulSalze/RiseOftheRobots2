"""Extract RISE 2 backgrounds, preserving their full 400/800-pixel width.

Usage: python TOOLS/extract_ggf.py [--source DIRECTORY] [--output DIRECTORY]
The image width includes the scenery outside the 320/640-pixel viewport.
Evidence: FUN_3c731 and FUN_3c9d0; documentation/07_decodage_images.md.
"""

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw
from lzw import LZW
from project_paths import ROOT, SOURCE

SRC = SOURCE
OUTDIR = ROOT / "EXTRACTED/ggf"
GEOMETRIES = {80000: (400, 200), 256000: (640, 400), 320000: (800, 400)}


def expand_6bit(value):
    if not 0 <= value <= 63:
        raise ValueError(f"Composante VGA 6 bits invalide : {value}")
    return (value << 2) | (value >> 4)


def parse_palette(raw):
    if not raw or len(raw) % 3 or len(raw) > 768:
        raise ValueError(f"Taille de palette invalide : {len(raw)}")
    return bytes(expand_6bit(v) for v in raw)


@dataclass(frozen=True)
class GGF:
    flag: int
    palette: bytes | None
    pixels: bytes
    size: tuple[int, int]
    end_bit: int


def read_ggf(path):
    data = Path(path).read_bytes()
    if not data:
        raise ValueError("Fichier GGF vide")
    # FUN_3c9d0 reads a palette only if the flag is nonzero.
    offset = 769 if data[0] else 1
    if len(data) < offset:
        raise ValueError(f"Palette tronquée : {len(data)-1}/768 octets")
    palette = parse_palette(data[1:offset]) if data[0] else None
    decoder = LZW(data, offset)
    try:
        pixels = decoder.decode(max_out=320000)
    except EOFError as exc:
        raise ValueError("Flux LZW tronqué avant EOI") from exc
    if len(pixels) not in GEOMETRIES:
        raise ValueError(f"Géométrie GGF inconnue : {len(pixels)} pixels")
    # All original files have one zero padding byte after the EOI byte.
    trailing = data[(decoder.bits.pos + 7) // 8:]
    if trailing not in (b"", b"\x00"):
        raise ValueError("Données inattendues après EOI")
    return GGF(data[0], palette, pixels, GEOMETRIES[len(pixels)], decoder.bits.pos)


def decode_ggf(path):
    """Compatibility with the original helper; errors are no longer swallowed."""
    decoded = read_ggf(path)
    return decoded.flag, decoded.palette, decoded.pixels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=SRC)
    parser.add_argument("--output", type=Path, default=OUTDIR)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    palette_dir = args.output.parent / "_palettes"
    palette_dir.mkdir(exist_ok=True)
    entries, thumbs = [], []
    for path in sorted(args.source.glob("*.GGF")):
        entry = {"file": path.name, "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
        try:
            decoded = read_ggf(path)
            if decoded.palette is None:
                raise ValueError("Palette héritée du jeu nécessaire pour le rendu couleur")
            image = Image.frombytes("P", decoded.size, decoded.pixels)
            image.putpalette(decoded.palette)
            image.save(args.output / f"{path.stem}.png")
            (palette_dir / f"{path.stem}.pal").write_bytes(decoded.palette)
            w, h = decoded.size
            entry.update(status="ok", width=w, height=h, flag=decoded.flag,
                         end_bit=decoded.end_bit, pixels_sha256=hashlib.sha256(decoded.pixels).hexdigest(),
                         centered_viewport=[(w-(320 if h == 200 else 640))//2, 0,
                                            320 if h == 200 else 640, h])
            thumb = image.convert("RGB")
            thumb.thumbnail((200, 100))
            thumbs.append((path.stem, thumb))
        except ValueError as exc:
            entry.update(status="invalid_source", error=str(exc))
            print(f"{path.name}: {exc}")
        entries.append(entry)
    report = {"decoded": len(thumbs), "rejected": len(entries)-len(thumbs), "files": entries}
    (args.output / "manifest.json").write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    if thumbs:
        sheet = Image.new("RGB", (1000, ((len(thumbs)+4)//5)*124), "#141921")
        draw = ImageDraw.Draw(sheet)
        for i, (name, thumb) in enumerate(thumbs):
            x, y = (i % 5)*200, (i // 5)*124
            sheet.paste(thumb, (x, y))
            draw.text((x+6, y+104), name, fill="white")
        sheet.save(args.output / "contact_sheet.jpg", quality=90)
    print(f"GGF : {len(thumbs)} images extraites, {report['rejected']} sources invalides (manifest.json)")


if __name__ == "__main__":
    main()
