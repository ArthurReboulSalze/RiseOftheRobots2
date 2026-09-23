"""Validate the complete ANL/ANR corpus, and round-trip every exported atlas.

Runs against the real game assets; no generated golden fixtures are required.
"""

import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path

from PIL import Image
from extract_ggf import ROOT, SRC, read_ggf
from sprite_codec import read_bank


def verify(source=SRC, extracted=ROOT / "EXTRACTED"):
    report = {"banks": [], "ggf": [], "atlas_frames": 0}
    for path in sorted(source.glob("*.ANL")):
        frames = read_bank(path)
        report["banks"].append({"name": path.stem, "frames": len(frames),
                                "empty": sum(not f.spans for f in frames),
                                "source_bytes": frames[-1].end,
                                "status": "all offsets and terminators exact"})
    for path in sorted(source.glob("*.GGF")):
        try:
            decoded = read_ggf(path)
        except ValueError as exc:
            report["ggf"].append({"name": path.name, "status": "invalid_source", "error": str(exc)})
            continue
        with Image.open(extracted / "ggf" / f"{path.stem}.png") as image:
            if image.mode != "P" or image.size != decoded.size or image.tobytes() != decoded.pixels:
                raise ValueError(f"GGF PNG differs from source pixels : {path.name}")
            if bytes(image.getpalette()) != decoded.palette:
                raise ValueError(f"GGF palette differs : {path.name}")
        report["ggf"].append({"name": path.name, "status": "exact_png_roundtrip"})
    for path in sorted((extracted / "sprites").glob("*/manifest.json")):
        manifest = json.loads(path.read_text(encoding="utf-8"))
        pages = [Image.open(path.parent / page).convert("RGBA") for page in manifest["pages"]]
        indices = [Image.open(path.parent / page) for page in manifest["index_pages"]]
        for f in manifest["frames"]:
            x,y,w,h = f["rect"]
            image = pages[f["page"]].crop((x,y,x+w,y+h))
            if hashlib.sha256(image.tobytes()).hexdigest() != f["rgba_sha256"]:
                raise ValueError(f"Corrupt atlas: {manifest['bank']}/{f['id']}")
            index_image = indices[f["page"]].crop((x,y,x+w,y+h))
            if hashlib.sha256(index_image.tobytes()).hexdigest() != f["indices_sha256"]:
                raise ValueError(f"Corrupt indices: {manifest['bank']}/{f['id']}")
            if hashlib.sha256(image.getchannel('A').tobytes()).hexdigest() != f["mask_sha256"]:
                raise ValueError(f"Corrupt mask: {manifest['bank']}/{f['id']}")
            report["atlas_frames"] += 1
        for page in pages + indices:
            page.close()
    report["summary"] = {"anr_banks": len(report["banks"]),
                          "anr_frames": sum(b["frames"] for b in report["banks"]),
                          "empty_frames": sum(b["empty"] for b in report["banks"]),
                          "ggf": dict(Counter(f["status"] for f in report["ggf"])),
                          "atlas_frames": report["atlas_frames"]}
    (extracted / "validation.json").write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps(report["summary"], indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=SRC)
    parser.add_argument("--extracted", type=Path, default=ROOT / "EXTRACTED")
    args = parser.parse_args()
    verify(args.source, args.extracted)
