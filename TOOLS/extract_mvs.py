# MVS movement-table-to-JSON converter. Semantics verified against the engine:
#   [MV S 01][u32 0x1C48][u32 0x1BFE][directory: 96 u32, ending at 0xFFFFFFFF]
#   32-byte descriptor: [+0,+4,+8] three animation sequences (speeds); [+C,+10,+14] three displacements
#     horizontal (int16/step); [+18] transitions (mask,target), ending at FFFF; [+1C] flags;
#     [+1D] automatic movement; [+1E] resume index; [+1F] parameter (four separate bytes).
#   sequence: 2 bytes/entry; image = 2*byte0 + (byte1&1); control = byte1>>1; byte0 == 0xFF ends it.
#   displacement: int16 per step (one per sequence image; zero means stationary).
# Output: EXTRACTED/data/mvs/<bank>.json plus a summary.
import argparse
import struct, os, json
from pathlib import Path
from project_paths import ROOT, SOURCE

SRC = str(SOURCE)
OUT = os.path.join(ROOT, "EXTRACTED", "data", "mvs")

def parse_sequence(d, ptr, be=False):
    entries = []
    while ptr + 2 <= len(d):
        b0, b1 = d[ptr], d[ptr+1]
        if b0 == 0xFF:
            entries.append(dict(end=True))
            return entries
        entries.append(dict(image=2*b0 + (b1 & 1), ctrl=b1 >> 1))
        ptr += 2
    return entries + [dict(truncated=True)]

def parse_movements(d, ptr, n, be=False):
    if ptr == 0 or ptr + 2*n > len(d):
        return []
    fmt = '>h' if be else '<h'
    return [struct.unpack_from(fmt, d, ptr + i*2)[0] for i in range(n)]

def parse_transitions(d, ptr, be=False):
    fmt = '>HH' if be else '<HH'
    out = []
    while ptr + 4 <= len(d):
        mask, target = struct.unpack_from(fmt, d, ptr)
        if mask >= 0x8000:  # negative first word ends the table
            break
        out.append(dict(mask=mask, target=target))
        ptr += 4
        if len(out) > 32:
            break
    return out

def parse_bank(path):
    d = open(path, 'rb').read()
    be = False
    if d[:4] == b'MVS\x01':
        pass
    elif d[:4] == b'\x01SVM':      # variante big-endian (RBMG/RBMN)
        be = True
    else:
        return None
    if be:
        v1, v2 = struct.unpack_from('>II', d, 4)
        offs, p = [], 12
        while p + 4 <= len(d):
            v = struct.unpack_from('>I', d, p)[0]
            if v == 0xFFFFFFFF:
                break
            offs.append(v)
            p += 4
    else:
        v1, v2 = struct.unpack_from('<II', d, 4)
        offs, p = [], 12
        while p + 4 <= len(d):
            v = struct.unpack_from('<I', d, p)[0]
            if v == 0xFFFFFFFF:
                break
            offs.append(v)
            p += 4
    moves = []
    for i, o in enumerate(offs):
        if o + 32 > len(d):
            return dict(error=f"descriptor {i} out of bounds")
        fmt = '>7I' if be else '<7I'
        s0, s1, s2, m0, m1, m2, tr = struct.unpack_from(fmt, d, o)
        seq0 = parse_sequence(d, s0, be)
        seq1 = parse_sequence(d, s1, be)
        seq2 = parse_sequence(d, s2, be)
        moves.append(dict(
            index=i,
            sequences=[seq0, seq1, seq2],
            movements=[parse_movements(d, ptr, len(seq) - int(bool(seq and seq[-1].get('end'))), be)
                       for ptr, seq in zip((m0, m1, m2), (seq0, seq1, seq2))],
            transitions=parse_transitions(d, tr, be) if tr else [],
            flags=hex(d[o+0x1c]), auto_move=d[o+0x1d], resume_index=d[o+0x1e], param=d[o+0x1f],
            descriptor_offset=o))
    return dict(magic='MVS' + ('-BE' if be else ''), dword4=hex(v1), dword8=hex(v2), count=len(offs), moves=moves)

def main():
    global SRC, OUT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=SRC)
    parser.add_argument('--output', type=Path, default=OUT)
    args = parser.parse_args()
    SRC, OUT = str(args.source), str(args.output)
    os.makedirs(OUT, exist_ok=True)
    ok = bad = 0
    summary = {}
    for f in sorted(os.listdir(SRC)):
        if not f.endswith('.MVS'):
            continue
        r = parse_bank(os.path.join(SRC, f))
        if r is None or 'error' in r:
            bad += 1
            summary[f] = r or "invalid magic"
            print("ERROR", f, r)
            continue
        json.dump(r, open(os.path.join(OUT, f[:-4] + '.json'), 'w'), indent=1)
        summary[f] = dict(count=r['count'])
        ok += 1
    json.dump(summary, open(os.path.join(OUT, 'resume.json'), 'w'), indent=1)
    print(f"MVS ok={ok} bad={bad}")
    if bad or not ok:
        raise ValueError("Incomplete MVS conversion")
    # Example: transitions for RBT0 movement 0.
    r = json.load(open(os.path.join(OUT, 'RBT0.json'), encoding='utf-8'))
    m0 = r['moves'][0]
    print("RBT0 move 0 images:", [e.get('image', 'END') for e in m0['sequences'][0]])
    print("  displacements:", m0['movements'][0])
    print("  transitions:", m0['transitions'])

if __name__ == "__main__":
    main()
