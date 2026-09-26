"""Decode available ANI movies with the DOS player's RLE/delta rules.

Low-res path: 0x351e6 -> 0x34bee -> 0x323d4/0x32474.
High-res path: 0x347bf -> 0x32171/0x322a2.
Usage: python TOOLS/extract_ani.py
"""

import argparse
from concurrent.futures import ProcessPoolExecutor
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
            raise ValueError(f"ANI truncated at 0x{self.pos:x} (requested {size} bytes)")
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


def decode(data: bytes, width: int, height: int, content_height: int,
           frame_limit: int | None = None) -> tuple[list[bytes], bytes]:
    reader = Reader(data)
    count = reader.u16()
    if not 1 <= count <= 256:
        raise ValueError(f"Invalid ANI frame count: {count}")
    if not 0 < content_height <= height or width <= 0:
        raise ValueError("Invalid ANI geometry")
    limit = count if frame_limit is None else min(count, frame_limit)
    if limit < 1:
        raise ValueError("Invalid ANI frame limit")
    pixels = bytearray(width * height)
    top = (height - content_height) // 2

    # 0x32171/0x323d4 skip the leading byte, then stop at the row width.
    # RZVICT has a stale command count; trusting it corrupts the palette.
    for row in range(content_height):
        reader.u8()
        line = bytearray()
        while len(line) < width:
            run = reader.s8()
            if run < 0:
                line.extend(reader.take(-run))
            else:
                line.extend(bytes([reader.u8()]) * run)
        if len(line) != width:
            raise ValueError(f"Initial row {row}: {len(line)} pixels instead of {width}")
        offset = (top + row) * width
        pixels[offset:offset + width] = line

    if reader.pos & 1:
        reader.take(1)
    palette_6bit = bytearray(reader.take(768))
    palette_6bit[0:3] = b"\0\0\0"  # The DOS reader forces black at index 0.
    if max(palette_6bit) > 63:
        raise ValueError("ANI palette outside 6-bit VGA range")
    palette = bytes((value << 2) | (value >> 4) for value in palette_6bit)
    frames = [bytes(pixels)]

    # 0x32474: changed lines use two-pixel words; a negative word skips
    # rows, while a positive word gives the packet count for the row.
    for frame_index in range(1, limit):
        changed_lines = reader.u16()
        if not 1 <= changed_lines <= height:
            raise ValueError(f"Frame {frame_index}: {changed_lines} changed rows")
        row = top
        for _ in range(changed_lines):
            packets = reader.u16()
            while packets & 0x8000:
                row += 0x10000 - packets
                packets = reader.u16()
            if not 1 <= packets <= width:
                raise ValueError(f"Frame {frame_index}, row {row}: {packets} packets")
            if not 0 <= row < height:
                raise ValueError(f"Frame {frame_index}: row {row} outside screen")
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
                    raise ValueError(f"Frame {frame_index}: packet exceeds row {row}")
                pixels[dst:dst + len(block)] = block
                dst += len(block)
            row += 1
        frames.append(bytes(pixels))

    if frame_limit is None and reader.pos != len(data):
        raise ValueError(f"{len(data) - reader.pos} unconsumed bytes at ANI end")
    return frames, palette


def movie_format(stem: str) -> tuple[int, int, int]:
    if stem in FORMATS:
        return FORMATS[stem]
    if len(stem) == 6 and stem[0] == "R" and stem[2:] in ("LINK", "LINL", "VICT", "VICL"):
        return (320, 200, 100) if stem.endswith(("LINL", "VICL")) else (640, 400, 200)
    raise ValueError(f"Unmapped ANI geometry: {stem}")


def movie_kind(stem: str) -> str:
    if stem == "LLOGO":
        return "intro"
    if stem in ("END", "ENL"):
        return "epilogue"
    return "ending" if stem.endswith(("VICT", "VICL")) else "linked"


def export_movie(job: tuple[Path, Path]) -> dict:
    source, output_root = job
    stem = source.stem.upper()
    width, height, content_height = movie_format(stem)
    data = source.read_bytes()
    declared = int.from_bytes(data[:2], "little")
    # Both native players stop before the two terminal records. Several
    # shipped banks have a truncated final record, which DOS never reads.
    playable = max(1, declared - 2)
    exported = declared if stem == "LLOGO" else playable
    frames, palette = decode(data, width, height, content_height, exported)
    output = output_root / stem
    output.mkdir(parents=True, exist_ok=True)
    for i, raw in enumerate(frames):
        image = Image.frombytes("P", (width, height), raw)
        image.putpalette(palette)
        image.save(output / f"frame_{i:03d}.png", compress_level=1)
    slow_frames = 11 if stem=="LLOGO" else 1 if stem in ("END", "ENL") else 6
    delays = [20 if i < slow_frames else 2 for i in range(playable)]
    delays[-1] = 25
    manifest = dict(source=source.name, source_sha256=hashlib.sha256(data).hexdigest(),
                    size=[width, height], content_height=content_height,
                    frame_count=len(frames), declared_frame_count=declared,
                    playable_frame_count=playable, kind=movie_kind(stem),
                    robot_slot=stem[1] if stem.startswith("R") else "",
                    timing_hz=25, frame_ticks=delays,
                    frames=[f"frame_{i:03d}.png" for i in range(len(frames))])
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return dict(name=stem, **{k: manifest[k] for k in
                ("source_sha256", "size", "frame_count", "playable_frame_count", "kind", "robot_slot")})


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, default=SOURCE_DIR)
    parser.add_argument("--output-dir", type=Path, default=OUTPUT_DIR)
    parser.add_argument("--workers", type=int, default=4)
    args = parser.parse_args()
    sources = sorted(p for p in args.source_dir.iterdir() if p.suffix.upper() == ".ANI")
    if not sources:
        raise ValueError("No ANI movies in the supplied game directory")
    movies = []
    with ProcessPoolExecutor(max_workers=max(1, min(4, args.workers))) as pool:
        for movie in pool.map(export_movie, ((p, args.output_dir) for p in sources)):
            movies.append(movie)
            print(f"{movie['name']}: {movie['frame_count']} frames", flush=True)
    # Director's Cut includes duplicated 'No Animation Yet...' source cards.
    # Identify only exact copies of its known card, in either resolution.
    cards = {m['source_sha256'] for m in movies if m['name'] in ('R2LINK', 'R2LINL')}
    for movie in movies:
        movie['placeholder'] = movie['source_sha256'] in cards
    args.output_dir.mkdir(parents=True, exist_ok=True)
    catalog_path=args.output_dir / "catalog.json"
    bonus=[]
    if catalog_path.exists():
        bonus=[movie for movie in json.loads(catalog_path.read_text(encoding='utf-8'))['movies']
               if movie.get('kind')=='bonus']
    catalog_path.write_text(json.dumps(dict(movies=movies+bonus), indent=2) + "\n", encoding="utf-8")
    print(f"{len(movies)} movies indexed -> {args.output_dir}")


if __name__ == "__main__":
    main()
