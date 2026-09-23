# Export RISE2_EXR strings with cross-references to ANALYSIS/exr_strings.txt.
from ghidra_session import open_rotr2

from project_paths import ROOT as PROJECT_ROOT, SOURCE, ANALYSIS
ROOT = str(PROJECT_ROOT)
OUT = os.path.join(ANALYSIS, "exr_strings.txt") if (os := __import__('os')) else None

project, program = open_rotr2()
listing = program.getListing()
rm = program.getReferenceManager()
lines = []
for d in listing.getDefinedData(True):
    t = d.getDataType().getName()
    if "string" not in t.lower():
        continue
    try:
        val = str(d.getValue())
    except Exception:
        continue
    if len(val) < 3:
        continue
    refs = []
    for r in rm.getReferencesTo(d.getAddress()):
        refs.append(str(r.getFromAddress()))
        if len(refs) >= 8:
            break
    lines.append("%s refs=[%s] %r" % (d.getAddress(), ",".join(refs), val))
open(OUT, "w", encoding="utf-8", errors="replace").write("\n".join(lines))
print("Strings:", len(lines), "->", OUT)
project.close()
