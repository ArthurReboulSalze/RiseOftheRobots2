# Export du listing assembleur complet (instructions + données) d'un programme Ghidra/PyGhidra.
# Usage : python TOOLS/export_listing.py <binary> <out_listing.txt>
import os, sys


import pyghidra

from project_paths import ROOT as PROJECT_ROOT, SOURCE, ANALYSIS
ROOT = str(PROJECT_ROOT)
BIN = sys.argv[1] if len(sys.argv) > 1 else str(SOURCE / "RISE2.EXE")
OUT = os.path.join(ANALYSIS, "launcher_disasm.txt")

pyghidra.start()

with pyghidra.open_program(
    BIN,
    project_location=os.path.join(ANALYSIS, "pyghidra_proj"),
    project_name="rotr2_scripts",
    analyze=False,
) as flat:
    program = flat.getCurrentProgram()
    listing = program.getListing()
    fm = program.getFunctionManager()
    with open(OUT, "w") as f:
        for func in fm.getFunctions(True):
            body = func.getBody()
            f.write(";; ===== %s @ %s =====\n" % (func.getName(), func.getEntryPoint()))
            addr = body.getMinAddress()
            it = listing.getInstructions(body, True)
            for ins in it:
                f.write("%s  %-28s %s\n" % (ins.getAddress(), ins.getMnemonicString(), ins.toString()))
            f.write("\n")
    print("LISTING OK ->", OUT)