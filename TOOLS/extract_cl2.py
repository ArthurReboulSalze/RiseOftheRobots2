# Convertisseur CL2 -> JSON. Format validé par asm_fn_15d92 :
#   [16 o header : magic CLL5/CLL6, dwords, u16 count @12, ...]
#   records depuis l'offset 16 :
#     rec[0]=b0, rec[1]=b1, rec[2]=b2 ; avance = 3 + b0*5 + b1*6 + b2*5
#     blocs : b0 blocs de 5 o, b1 blocs de 6 o, b2 blocs de 5 o
# Sortie : EXTRACTED/data/cl2/<name>.json + resume cl2.json
import argparse
import struct, os, json
from pathlib import Path
from project_paths import ROOT, SOURCE

SRC = str(SOURCE)
OUT = os.path.join(ROOT, "EXTRACTED", "data", "cl2")

def parse(path):
    d = open(path, 'rb').read()
    if len(d) < 16 or d[:3] != b'CLL':
        return None
    magic = d[:4].decode('ascii', 'replace')
    v8 = struct.unpack_from('<I', d, 4)[0]
    vers = struct.unpack_from('<H', d, 8)[0]
    w10 = struct.unpack_from('<H', d, 10)[0]
    count = struct.unpack_from('<H', d, 12)[0]
    w14 = struct.unpack_from('<H', d, 14)[0]
    off = 16
    records = []
    for i in range(count):
        if off + 3 > len(d):
            return dict(error="fin prématurée", at=i, off=off)
        b0, b1, b2 = d[off], d[off+1], d[off+2]
        size = 3 + b0*5 + b1*6 + b2*5
        if off + size > len(d):
            return dict(error="record déborde", at=i, off=off, b0=b0, b1=b1, b2=b2)
        body = d[off+3:off+size]
        b0_blocks = [body[j*5:(j+1)*5] for j in range(b0)]
        b1_blocks = [body[b0*5+j*6:b0*5+(j+1)*6] for j in range(b1)]
        b2_blocks = [body[b0*5+b1*6+j*5:b0*5+b1*6+(j+1)*5] for j in range(b2)]
        records.append(dict(index=i, rec_offset=off,
            attack_boxes=[dict(x=b[0], y=b[1], w=b[2], h=b[3], damage_or_type=b[4]) for b in b0_blocks],
            body_boxes=[dict(x=b[0], y=b[1], w=b[2], h=b[3], p4=b[4], part=b[5]) for b in b1_blocks],
            single_box=[dict(x=b[0], y=b[1], w=b[2], h=b[3], tag=b[4]) for b in b2_blocks]))
        off += size
    consumed = off
    return dict(magic=magic, dword4=v8, word8=vers, word10=w10, count=count, word14=w14,
                consumed=consumed, filesize=len(d), exact_fit=(consumed == len(d)),
                records=records)

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
        if not f.endswith('.CL2'):
            continue
        r = parse(os.path.join(SRC, f))
        if r is None:
            continue
        if r.get('error'):
            bad += 1
            summary[f] = dict(status='ERREUR', detail=r)
        else:
            ok += 1
            summary[f] = dict(status='ok', magic=r['magic'], count=r['count'],
                              exact_fit=r['exact_fit'], consumed=r['consumed'],
                              filesize=r['filesize'])
            json.dump(r, open(os.path.join(OUT, f.replace('.CL2', '.json')), 'w'), indent=1)
    json.dump(summary, open(os.path.join(OUT, '..', 'cl2_resume.json'), 'w'), indent=1)
    print(f"CL2 ok={ok} bad={bad}")
    for f, s in summary.items():
        if s['status'] != 'ok':
            print("  ERREUR", f, s.get('detail'))
    if bad or not ok:
        raise ValueError("Conversion CL2 incomplète")

if __name__ == "__main__":
    main()
