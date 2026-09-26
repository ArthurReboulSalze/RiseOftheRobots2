"""Export combat effect/reaction tables from a user-supplied DOS executable."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from le_codec import flatten
from project_paths import ROOT, SOURCE

BUILDS = {
    '213ba86c2292cb3203f87826c030fbdb9d4c77c3a8c27d69fce230a53dc4241b': 0,
    'ea6730a421bcd6f7a8aefcfdec59c2b6ee1e91430305b72cb96c0a0fa5b5d135': 4,
}


def parse_tables(raw):
    digest = hashlib.sha256(raw).hexdigest()
    if digest not in BUILDS:
        raise ValueError('Unmapped EXR combat tables; analyse this edition first')
    image, metadata = flatten(raw)
    base, shift = metadata['base'], BUILDS[digest]

    def read(address, fmt):
        size = struct.calcsize(fmt)
        offset = address - base
        if not 0 <= offset <= len(image) - size:
            raise ValueError('Combat table out of bounds')
        return struct.unpack_from(fmt, image, offset)

    def sequence(address, stride):
        out = []
        for i in range(512):
            value = read(address + i*stride, '<h')[0]
            if value == -1:
                return out
            if not 0 <= value < 280:
                raise ValueError('Invalid EXTRA effect frame')
            if stride == 2:
                out.append(value)
            else:
                _, dx, dy = read(address + i*stride, '<3h')
                out.append(dict(image=value, dx=dx, dy=dy, flags=2))
        raise ValueError('Unterminated combat effect')

    return dict(executable_sha256=digest,
                impacts=[sequence(p, 6) for p in read(0x62c66 + shift, '<4I')],
                particles=[sequence(p, 2) for p in read(0x62702, '<16I')],
                super_strength=list(read(0x62b0a + shift, '<30h' if shift else '<28h')),
                reactions=list(read(0x62b48 + shift, '<4h')))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=SOURCE)
    parser.add_argument('--output', type=Path, default=ROOT / 'EXTRACTED/data/combat.json')
    args = parser.parse_args()
    data = parse_tables((args.source / 'RISE2.EXR').read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
    print('Exported four impact scripts, sixteen particle sequences and combat tables')


if __name__ == '__main__':
    main()
