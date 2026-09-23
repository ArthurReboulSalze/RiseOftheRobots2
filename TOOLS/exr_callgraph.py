# Export the RISE2_EXR call graph and function list.
# -> ANALYSIS/exr_functions.txt, ANALYSIS/exr_callgraph.txt
from ghidra_session import open_rotr2
import os, sys

from project_paths import ROOT as PROJECT_ROOT, SOURCE, ANALYSIS
ROOT = str(PROJECT_ROOT)
OUT = str(ANALYSIS)

project, program = open_rotr2()
fm = program.getFunctionManager()
rm = program.getReferenceManager()

funcs = list(fm.getFunctions(True))
# Function list with sizes.
with open(os.path.join(OUT, "exr_functions.txt"), "w") as f:
    for fn in funcs:
        f.write("%s  %-20s size=%d\n" % (fn.getEntryPoint(), fn.getName(), fn.getBody().getNumAddresses()))

# Call graph: callers -> callees.
with open(os.path.join(OUT, "exr_callgraph.txt"), "w") as f:
    for fn in funcs:
        callees = fn.getCalledFunctions(monitor) if (monitor := None) is None else None
        # Fallback: follow CALL references in the function body.
        callees = set()
        from ghidra.program.model.symbol import RefType
        for addr in fn.getBody().getAddresses(True):
            for ref in rm.getReferencesFrom(addr):
                if ref.getReferenceType().isCall():
                    callees.add(ref.getToAddress().getOffset())
        ep = fn.getEntryPoint().getOffset()
        f.write("%x -> %s\n" % (ep, " ".join("%x" % c for c in sorted(callees)) if callees else ""))
print("Functions:", len(funcs))
project.close()
