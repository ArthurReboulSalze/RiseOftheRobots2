# Parsers des formats de données Phase 1 (structures validées, sémantique complète à affiner en Phase 2).
# Sorties : EXTRACTED/data/*.json
import struct, os, json

from project_paths import ROOT as PROJECT_ROOT, SOURCE, ANALYSIS
ROOT = str(PROJECT_ROOT)
SRC = str(SOURCE)
OUT = os.path.join(ROOT, "EXTRACTED", "data")


def save(name, obj):
    with open(os.path.join(OUT, name), "w", encoding="utf-8") as f:
        json.dump(obj, f, indent=1, ensure_ascii=False)
    print("  ->", name)


def parse_aip(path):
    d = open(path, 'rb').read()
    n1, n2 = struct.unpack_from('<II', d, 0)
    if 8 + n1*14 + n2*4 != len(d):
        return None
    off = 8
    moves = []
    for i in range(n1):
        a, b1, b2, b3, c1, c2, c3, b4 = struct.unpack_from('<IBBBHHHB', d, off)
        off += 14
        moves.append(dict(index=i, flags=a, b=[b1, b2, b3], u16=[c1, c2, c3], tail=b4))
    extra = list(struct.unpack_from(f'<{n2}I', d, off))
    return dict(file=os.path.basename(path), n_moves=n1, n_extra=n2,
                moves=moves, extra_dwords=[hex(v) for v in extra])


def parse_mrw(path):
    d = open(path, 'rb').read()
    cnt = struct.unpack_from('<H', d, 0)[0]
    chunks = []
    for i in range(cnt):
        a, b = struct.unpack_from('<II', d, 2 + i*8)
        chunks.append(dict(offset=a, size=b))
    return dict(file=os.path.basename(path), count=cnt, chunks=chunks,
                filesize=len(d),
                note="paires (offset, taille) ; enchainement verifie sur BGA (130+963=1093, +1894=2987, +41344=fin)")


def parse_mvs(path):
    d = open(path, 'rb').read()
    magic = d[:4]
    if magic[:3] != b'MVS':
        return None
    v1, v2 = struct.unpack_from('<II', d, 4)
    # table d'entrees de 32 octets a partir de l'offset dword[2] jusqu'au 0xFFFFFFFF
    n_dir = (0x190 - 12)//4 if False else None
    # lecture generique : dwords depuis 12 jusqu'au premier 0xFFFFFFFF
    offs = []
    p = 12
    while p + 4 <= len(d):
        v = struct.unpack_from('<I', d, p)[0]
        if v == 0xFFFFFFFF:
            p += 4
            break
        offs.append(v)
        p += 4
    entries = []
    for o in offs:
        e = struct.unpack_from('<8I', d, o) if o + 32 <= len(d) else None
        entries.append(e)
    return dict(file=os.path.basename(path), magic=magic.decode(), v1=v1, v2=v2,
                dir_count=len(offs), dir_first=offs[0] if offs else None,
                entries=[[hex(x) for x in e] if e else None for e in entries])


def parse_sound_dat(path):
    d = open(path, 'rb').read()
    return dict(file=os.path.basename(path), size=len(d),
                u16=list(struct.unpack_from(f'<{len(d)//2}H', d, 0)))


def parse_sts(path):
    d = open(path, 'rb').read()
    return dict(file=os.path.basename(path), size=len(d),
                u16=list(struct.unpack_from(f'<{len(d)//2}H', d, 0)))


def parse_ctl(path):
    d = open(path, 'rb').read()
    dw = list(struct.unpack_from(f'<{len(d)//4}I', d, 0))
    return dict(file=os.path.basename(path), size=len(d), dwords=[hex(v) for v in dw])


def parse_cl2(path):
    d = open(path, 'rb').read()
    return dict(file=os.path.basename(path), magic=d[:4].decode('ascii', 'replace'),
                size=len(d), head=d[:40].hex(' '))


def main():
    global SRC, OUT
    import argparse
    p = argparse.ArgumentParser()
    p.add_argument("--source", default=SRC)
    p.add_argument("--output", default=OUT)
    args = p.parse_args()
    SRC, OUT = args.source, args.output
    os.makedirs(OUT, exist_ok=True)
    aips, mrws, mvss, ctls, cl2s = {}, {}, {}, {}, {}
    for f in sorted(os.listdir(SRC)):
        p = os.path.join(SRC, f)
        if f.endswith('.AIP'):
            aips[f] = parse_aip(p)
        elif f.endswith('.MRW'):
            mrws[f] = parse_mrw(p)
        elif f.endswith('.MVS'):
            mvss[f] = parse_mvs(p)
        elif f.endswith('.CTL'):
            ctls[f] = parse_ctl(p)
        elif f.endswith('.CL2'):
            cl2s[f] = parse_cl2(p)
    json.dump(aips, open(os.path.join(OUT, 'aip.json'), 'w'), indent=1)
    json.dump(mrws, open(os.path.join(OUT, 'mrw.json'), 'w'), indent=1)
    json.dump(mvss, open(os.path.join(OUT, 'mvs.json'), 'w'), indent=1)
    json.dump(ctls, open(os.path.join(OUT, 'ctl.json'), 'w'), indent=1)
    json.dump(cl2s, open(os.path.join(OUT, 'cl2.json'), 'w'), indent=1)
    sts = {}
    for f in sorted(os.listdir(SRC)):
        if f.endswith('.STS'):
            d = open(os.path.join(SRC, f), 'rb').read()
            sts[f] = list(struct.unpack_from(f'<{len(d)//2}H', d, 0))
    json.dump(sts, open(os.path.join(OUT, 'sts.json'), 'w'), indent=1)
    d = open(os.path.join(SRC, 'SOUND.DAT'), 'rb').read()
    json.dump(dict(size=len(d), u16=list(struct.unpack_from(f'<{len(d)//2}H', d, 0))),
              open(os.path.join(OUT, 'sound_dat.json'), 'w'), indent=1)
    print("aip:", len(aips), "mrw:", len(mrws), "mvs:", len(mvss), "ctl:", len(ctls), "cl2:", len(cl2s))
    print("-> EXTRACTED/data/")


if __name__ == "__main__":
    main()