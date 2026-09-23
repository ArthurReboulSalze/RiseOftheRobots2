# Catalogueur de fichiers de données RISE2 : taille, premiers octets, entropie.
# Usage : python TOOLS/catalog.py [data_dir]  → ANALYSIS/catalog.csv + ANALYSIS/catalog.md
import os, sys, math, collections

from project_paths import SOURCE, ANALYSIS
ANALYSIS.mkdir(parents=True, exist_ok=True)
DATA = sys.argv[1] if len(sys.argv) > 1 else str(SOURCE)
OUT_CSV = str(ANALYSIS / "catalog.csv")
OUT_MD = str(ANALYSIS / "catalog.md")

def entropy(b):
    if not b: return 0.0
    counts = collections.Counter(b)
    n = len(b)
    return -sum((c / n) * __import__('math').log2(c / n) for c in counts.values())

rows = []
for f in sorted(os.listdir(DATA)):
    p = os.path.join(DATA, f)
    if not os.path.isfile(p):
        continue
    data = open(p, 'rb').read(1 << 16)
    size = os.path.getsize(p)
    head = data[:16]
    ent = entropy(data[:8192])
    rows.append((f, size, head.hex(' '), ent))

with open(OUT_CSV, 'w', encoding='utf-8') as fo:
    fo.write("name,size,head_hex,entropy\n")
    for f, size, head, ent in rows:
        fo.write(f"{f},{size},{head.replace(' ','')},{ent:.3f}\n")

# résumé par extension
groups = collections.defaultdict(list)
for f, size, head, ent in rows:
    ext = os.path.splitext(f)[1].lower() or '(none)'
    groups[ext].append((f, size, head, ent))

with open(OUT_MD, 'w', encoding='utf-8') as fo:
    fo.write("# Catalogue des fichiers de données\n\nRépertoire : `%s`\n\n" % DATA)
    for ext in sorted(groups, key=lambda e: -len(groups[e])):
        items = groups[ext]
        sizes = [s for _, s, _, _ in items]
        fo.write(f"## {ext} — {len(items)} fichiers\n\n")
        fo.write(f"tailles : min={min(sizes)}, max={max(sizes)}, total={sum(sizes)}\n\n")
        fo.write("| fichier | taille | entropie(8K) | 16 premiers octets |\n|---|---|---|---|\n")
        for f, size, head, ent in items[:40]:
            fo.write(f"| {f} | {size} | {ent:.2f} | `{head}` |\n")
        if len(items) > 40:
            fo.write(f"\n_(+{len(items) - 40} autres)_\n")
        fo.write("\n")
print("catalogue écrit :", OUT_CSV, "et", OUT_MD)