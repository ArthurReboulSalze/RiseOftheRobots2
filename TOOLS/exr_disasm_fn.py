# Export RISE2_EXR function assembly to ANALYSIS/funcs/asm_fn_<addr>.txt.
# Usage : python TOOLS/exr_disasm_fn.py <hex...>
from ghidra_session import open_rotr2
import os, sys

from project_paths import ROOT as PROJECT_ROOT, SOURCE, ANALYSIS
ROOT = str(PROJECT_ROOT)
OUTDIR = os.path.join(ANALYSIS, "funcs")
ADDRS = [int(a, 16) for a in sys.argv[1:]]

project, program = open_rotr2()
fm = program.getFunctionManager()
af = program.getAddressFactory().getDefaultAddressSpace()
listing = program.getListing()
for a in ADDRS:
    addr = af.getAddress(a)
    fn = fm.getFunctionAt(addr) or fm.getFunctionContaining(addr)
    if fn is None:
        print("No function at", hex(a))
        continue
    path = os.path.join(OUTDIR, "asm_fn_%x.txt" % fn.getEntryPoint().getOffset())
    with open(path, "w") as f:
        for ins in listing.getInstructions(fn.getBody(), True):
            f.write("%s  %-6s %s\n" % (ins.getAddress(), ins.getMnemonicString(), ins.toString()))
    print("asm_fn_%x.txt (%d bytes)" % (fn.getEntryPoint().getOffset(), fn.getBody().getNumAddresses()))
project.close()
