"""Export all robot frames (both resolutions) and versus portraits to PNG atlases.

python TOOLS/extract_anr.py
python TOOLS/extract_anr.py --banks RBTA RB4A --individual
Each atlas has a JSON manifest with original coordinates and exact pixel hashes.
"""

import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw

from extract_ggf import ROOT, SRC, parse_palette
from sprite_codec import read_bank, render_frame, indexed_frame

ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123"


def palette_for(source, stem, frame_id):
    if stem.startswith(("RBT", "RB4")) and len(stem) == 4:
        path = source / f"R{stem[-1]}.PAL"
        # The game loads 70 colors per player (0xd2 bytes), not all 80 in the file.
        raw = path.read_bytes()[:210]
        # Preserve the arena/effects colors at 140..255. The opponent bank is
        # shown with this same robot's colors; index atlases retain runtime data.
        background = source / f"AG{stem[-1]}.GGF"
        context = bytearray(parse_palette(background.read_bytes()[1:769])) if background.exists() else bytearray(768)
        context[:210] = parse_palette(raw)
        context[210:420] = parse_palette(raw)
        label = f"{path.name}[0:70] + same-robot opponent"
        return bytes(context), label + (f" + {background.name}[140:256]" if background.exists() else " (no effect indices)")
    elif stem in ("VSFACE", "V4FACE"):
        path = source / "VSFACE.PAL"
        raw = path.read_bytes()[frame_id*210:(frame_id+1)*210]
        label = f"{path.name}@{frame_id*210}"
    else:
        raise ValueError(f"Palette not yet assigned for {stem}")
    if len(raw) != 210:
        raise ValueError(f"Incomplete palette : {label}")
    rgb = parse_palette(raw)
    return rgb + bytes(768-len(rgb)), label


def save_json(path, obj):
    path.write_text(json.dumps(obj, indent=2, ensure_ascii=False), encoding="utf-8")


def export_bank(path, output, individual=False, page_size=2048):
    frames = read_bank(path)
    stem = path.stem
    if stem.startswith(("RBT", "RB4")) and not (path.parent / f"AG{stem[-1]}.GGF").exists():
        if any(v >= 140 for frame in frames for span in frame.spans for v in span.pixels):
            raise ValueError(f"Background palette required for effects in {stem}")
    dest = output / stem
    dest.mkdir(parents=True, exist_ok=True)
    is_low = stem.startswith("RB4") or stem == "V4FACE"
    canvas_size = (320, 200) if is_low else (640, 400)
    manifest = {"bank": stem, "source_size": list(canvas_size), "frame_count": len(frames),
                "source_anr_sha256": hashlib.sha256(path.with_suffix('.ANR').read_bytes()).hexdigest(),
                "source_anl_sha256": hashlib.sha256(path.with_suffix('.ANL').read_bytes()).hexdigest(),
                "coordinate_system": "top-left; original x/y retained in origin; no gameplay timing implied",
                "pages": [], "index_pages": [], "frames": []}
    page = Image.new("RGBA", (page_size, page_size))
    index_page = Image.new("L", (page_size, page_size))
    x = y = 1
    row_height = used_width = used_height = 0
    page_id = 0
    preview = None

    def flush():
        name = f"atlas_{page_id:03}.png"
        page.crop((0, 0, used_width+1, used_height+1)).save(dest / name)
        manifest["pages"].append(name)
        index_name = f"indices_{page_id:03}.png"
        index_page.crop((0, 0, used_width+1, used_height+1)).save(dest / index_name)
        manifest["index_pages"].append(index_name)

    if individual:
        (dest / "frames").mkdir(exist_ok=True)
    for i, frame in enumerate(frames):
        palette, palette_label = palette_for(path.parent, stem, i)
        indices, mask = indexed_frame(frame)
        image = render_frame(frame, palette)
        w, h = image.size
        if max(w, h) + 2 > page_size:
            raise ValueError("Frame too large for the atlas")
        if x + w + 1 > page_size:
            x = 1
            y += row_height + 2
            row_height = 0
        if y + h + 1 > page_size:
            flush()
            page_id += 1
            page = Image.new("RGBA", (page_size, page_size))
            index_page = Image.new("L", (page_size, page_size))
            x = y = 1
            row_height = used_width = used_height = 0
        page.paste(image, (x, y))
        index_page.paste(indices, (x, y))
        bbox = frame.bbox
        record = {"id": i, "offset": frame.offset, "bytes": frame.end-frame.offset,
                  "empty": bbox is None, "origin": list(bbox[:2]) if bbox else [0, 0],
                  "bbox": list(bbox) if bbox else None, "page": page_id, "rect": [x,y,w,h],
                  "palette": palette_label, "rgba_sha256": hashlib.sha256(image.tobytes()).hexdigest(),
                  "indices_sha256": hashlib.sha256(indices.tobytes()).hexdigest(),
                  "mask_sha256": hashlib.sha256(mask.tobytes()).hexdigest(),
                  "uses_opponent_palette": any(70 <= v < 140 for s in frame.spans for v in s.pixels)}
        if stem in ("VSFACE", "V4FACE"):
            if i >= len(ALPHABET):
                raise ValueError(f"Portrait {i} has no known mapping in {stem}")
            record["robot_slot"] = ALPHABET[i]
        manifest["frames"].append(record)
        if individual:
            image.save(dest / "frames" / f"{i:04}.png")
        if preview is None and bbox is not None:
            preview = image.copy()
            preview.save(dest / "preview.png")
        x += w + 2
        row_height = max(row_height, h)
        used_width, used_height = max(used_width, x-2), max(used_height, y+h)
    flush()
    save_json(dest / "manifest.json", manifest)
    return manifest, preview


def make_roster(entries, output):
    cell_w, cell_h = 200, 230
    sheet = Image.new("RGB", (7*cell_w, ((len(entries)+6)//7)*cell_h), "#101722")
    draw = ImageDraw.Draw(sheet)
    for index, (manifest, preview) in enumerate(entries):
        x, y = index % 7 * cell_w, index // 7 * cell_h
        draw.rectangle((x+4,y+4,x+cell_w-5,y+cell_h-5), fill="#1b2635")
        image = preview.copy()
        image.thumbnail((180, 186), Image.Resampling.NEAREST)
        sheet.paste(image, (x+(cell_w-image.width)//2, y+10+(186-image.height)//2), image)
        draw.text((x+12,y+201), f"{manifest['bank']}  |  {manifest['frame_count']} frames", fill="#d4dfef")
    sheet.save(output / "robots.png")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=SRC)
    parser.add_argument("--output", type=Path, default=ROOT / "EXTRACTED/sprites")
    parser.add_argument("--banks", nargs="+")
    parser.add_argument("--individual", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    stems = args.banks or [stem for stem in ([f"RBT{slot}" for slot in ALPHABET] +
                           [f"RB4{slot}" for slot in ALPHABET] + ["VSFACE", "V4FACE"])
                           if (args.source / f"{stem}.ANL").exists()]
    catalog, roster = [], []
    for stem in stems:
        manifest, preview = export_bank(args.source / f"{stem}.ANL", args.output, args.individual)
        catalog.append({k:manifest[k] for k in ("bank", "source_size", "frame_count", "pages")})
        if stem.startswith("RBT"):
            roster.append((manifest, preview))
        print(f"{stem}: {manifest['frame_count']} frames, {len(manifest['pages'])} atlas pages", flush=True)
    save_json(args.output / "catalog.json", catalog)
    if roster:
        make_roster(roster, args.output)
    print(f"Total: {len(catalog)} banks, {sum(m['frame_count'] for m in catalog)} frames")


if __name__ == "__main__":
    main()
