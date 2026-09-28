"""Read each robot's initial special-power icon from an owned RISE2.EXR.

FUN_132b6 indexes the 18-word table at 0x50660 by robot slot and sets the
matching bit in each fighter's power mask. FUN_16d4e turns set bits 0..5 into
CHRSET characters 0x76..0x7b. Bonus/hidden slots have no proven table entries.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct

from le_codec import flatten
from project_paths import SOURCE


KNOWN_EXR = {
    "213ba86c2292cb3203f87826c030fbdb9d4c77c3a8c27d69fce230a53dc4241b": "DOS",
    "ea6730a421bcd6f7a8aefcfdec59c2b6ee1e91430305b72cb96c0a0fa5b5d135": "Director's Cut Disc 1",
}
TABLE_ADDRESS = 0x50660
SLOTS = "ABCDEFGHIJKLMNOPQR"


def parse_icons(image: bytes, base: int) -> dict[str, int]:
    offset = TABLE_ADDRESS - base
    if offset < 0 or offset + 2 * len(SLOTS) > len(image):
        raise ValueError("Power-icon table lies outside the EXR image")
    indices = struct.unpack_from(f"<{len(SLOTS)}H", image, offset)
    if any(index > 5 for index in indices):
        raise ValueError("Initial power-icon index outside the six source glyphs")
    return dict(zip(SLOTS, indices))


def main(argv=None) -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=SOURCE)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    raw = (args.source / "RISE2.EXR").read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest not in KNOWN_EXR:
        raise ValueError("Unmapped RISE2.EXR edition; inspect its power-icon table first")
    image, metadata = flatten(raw)
    icons = parse_icons(image, metadata["base"])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({"edition": KNOWN_EXR[digest],
                                       "initial_power_icons": icons}, indent=2) + "\n",
                           encoding="utf-8")
    print(f"{KNOWN_EXR[digest]}: {len(icons)} initial power icons -> {args.output}")


if __name__ == "__main__":
    main()
