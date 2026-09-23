"""Decode all supplied ANI movies with the DOS player's RLE/delta rules.

Low-res path: 0x351e6 -> 0x34bee -> 0x323d4/0x32474.
High-res path: 0x347bf -> 0x32171/0x322a2.
Usage: python TOOLS/extract_ani.py
"""

import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image
from project_paths import ROOT, SOURCE


SOURCE_DIR = SOURCE
OUTPUT_DIR = ROOT / "EXTRACTED/video"
FORMATS = {
    "LLOGO": (320, 200, 199),
    "END": (640, 400, 200),
    "ENL": (320, 200, 100),
}


class Reader:
    def __init__(self, data: bytes):
        self.data = data
        self.pos = 0

    def take(self, size: int) -> bytes:
        if size < 0 or self.pos + size > len(self.data):
            raise ValueError(f"ANI tronque a 0x{self.pos:x} (demande {size} octets)")
        result = self.data[self.pos:self.pos + size]
        self.pos += size
        return result

    def u8(self) -> int:
        return self.take(1)[0]

    def s8(self) -> int:
        value = self.u8()
        return value - 256 if value >= 128 else value

    def u16(self) -> int:
        return int.from_bytes(self.take(2), "little")


def decode(data: bytes, width: int, height: int, content_height: int) -> tuple[list[bytes], bytes]:
    reader = Reader(data)
    count = reader.u16()
    if not 1 <= count <= 256:
        raise ValueError(f"nombre de frames ANI invalide : {count}")
    pixels = bytearray(width * height)
    top = (height - content_height) // 2

    # 0x323d4 : une ligne = octet de nombre de commandes, puis RLE signe.
    for row in range(content_height):
        commands = reader.u8()
        line = bytearray()
        for _ in range(commands):
            run = reader.s8()
            if run < 0:
                line.extend(reader.take(-run))
            else:
                line.extend(bytes([reader.u8()]) * run)
        if len(line) != width:
            raise ValueError(f"ligne initiale {row} : {len(line)} pixels au lieu de {width}")
        offset = (top + row) * width
        pixels[offset:offset + width] = line

    if reader.pos & 1:
        reader.take(1)
    palette_6bit = bytearray(reader.take(768))
    palette_6bit[0:3] = b"\0\0\0"  # le lecteur DOS force le noir sur l'index 0
    if max(palette_6bit) > 63:
        raise ValueError("palette ANI hors plage VGA 6 bits")
    palette = bytes((value << 2) | (value >> 4) for value in palette_6bit)
    frames = [bytes(pixels)]

    # 0x32474 : lignes modifiees en mots de 2 pixels ; un mot negatif saute
    # des lignes, un mot positif donne le nombre de paquets de la ligne.
    for frame_index in range(1, count):
        changed_lines = reader.u16()
        if not 1 <= changed_lines <= height:
            raise ValueError(f"frame {frame_index} : {changed_lines} lignes modifiees")
        row = top
        for _ in range(changed_lines):
            packets = reader.u16()
            while packets & 0x8000:
                row += 0x10000 - packets
                packets = reader.u16()
            if not 1 <= packets <= width:
                raise ValueError(f"frame {frame_index}, ligne {row} : {packets} paquets")
            if not 0 <= row < height:
                raise ValueError(f"frame {frame_index} : ligne {row} hors ecran")
            dst = row * width
            for _ in range(packets):
                dst += reader.u8()
                run = reader.s8()
                if run <= 0:
                    pair = reader.take(2)
                    block = pair * -run
                else:
                    block = reader.take(run * 2)
                if dst + len(block) > (row + 1) * width:
                    raise ValueError(f"frame {frame_index} : paquet depasse la ligne {row}")
                pixels[dst:dst + len(block)] = block
                dst += len(block)
            row += 1
        frames.append(bytes(pixels))

    if reader.pos != len(data):
        raise ValueError(f"{len(data) - reader.pos} octets non consommes a la fin d'ANI")
    return frames, palette


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, default=SOURCE_DIR)
    parser.add_argument("--output-dir", type=Path, default=OUTPUT_DIR)
    args = parser.parse_args()
    for stem, (width, height, content_height) in FORMATS.items():
        source = args.source_dir / f"{stem}.ANI"
        output = args.output_dir / stem
        data = source.read_bytes()
        frames, palette = decode(data, width, height, content_height)
        output.mkdir(parents=True, exist_ok=True)
        for i, raw in enumerate(frames):
            image = Image.frombytes("P", (width, height), raw)
            image.putpalette(palette)
            image.save(output / f"frame_{i:03d}.png", optimize=True)
        manifest = {
            "source": source.name,
            "source_sha256": hashlib.sha256(data).hexdigest(),
            "size": [width, height],
            "content_height": content_height,
            "frame_count": len(frames),
            "frames": [f"frame_{i:03d}.png" for i in range(len(frames))],
        }
        (output / "manifest.json").write_text(
            json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
        )
        print(f"{len(frames)} frames extraites -> {output}")


if __name__ == "__main__":
    main()
