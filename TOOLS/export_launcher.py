# Export de l'analyse du lanceur RISE2.EXE via PyGhidra (projet privé, sans toucher au projet GUI).
# Sorties : ANALYSIS/launcher_functions.txt, ANALYSIS/launcher_decompiled.c, ANALYSIS/launcher_strings.txt
import os, sys


import pyghidra

from project_paths import ROOT as PROJECT_ROOT, SOURCE, ANALYSIS
ROOT = str(PROJECT_ROOT)
BIN = str(SOURCE / "RISE2.EXE")
OUT = str(ANALYSIS)

pyghidra.start()
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

with pyghidra.open_program(
    BIN,
    project_location=os.path.join(OUT, "pyghidra_proj"),
    project_name="rotr2_scripts",
    analyze=True,
) as flat:
    program = flat.getCurrentProgram()
    fm = program.getFunctionManager()
    listing = program.getListing()
    monitor = ConsoleTaskMonitor()

    di = DecompInterface()
    di.openProgram(program)

    with open(os.path.join(OUT, "launcher_functions.txt"), "w") as fl, \
         open(os.path.join(OUT, "launcher_decompiled.c"), "w") as dc:
        for f in fm.getFunctions(True):
            body = f.getBody()
            fl.write("%s  %s  (size=%d)\n" % (f.getEntryPoint(), f.getName(), body.getNumAddresses()))
            res = di.decompileFunction(f, 60, monitor)
            dc.write("// ===== %s @ %s =====\n" % (f.getName(), f.getEntryPoint()))
            if res.decompileCompleted():
                dc.write(res.getDecompiledFunction().getC())
            else:
                dc.write("// DECOMP FAILED: %s\n" % res.getErrorMessage())
            dc.write("\n\n")

    with open(os.path.join(OUT, "launcher_strings.txt"), "w") as sf:
        for d in listing.getDefinedData(True):
            tname = d.getDataType().getName()
            if "string" not in tname.lower():
                continue
            try:
                val = d.getValue()
            except Exception:
                continue
            if val is None or len(str(val)) < 4:
                continue
            refs = []
            rm = program.getReferenceManager()
            for r in rm.getReferencesTo(d.getAddress()):
                refs.append(str(r.getFromAddress()))
                if len(refs) >= 6:
                    break
            sf.write("%s  refs=[%s]  %r\n" % (d.getAddress(), ",".join(refs), str(val)))

    print("EXPORT OK: %d functions" % fm.getFunctionCount())