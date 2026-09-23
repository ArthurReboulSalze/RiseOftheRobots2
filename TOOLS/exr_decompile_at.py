# Decompile functions containing selected addresses (format-string references).
# Usage: python TOOLS/exr_decompile_at.py -> ANALYSIS/fn_<addr>.c for each unique function.
from ghidra_session import open_rotr2
import os, sys

from project_paths import ROOT as PROJECT_ROOT, SOURCE, ANALYSIS
ROOT = str(PROJECT_ROOT)
OUTDIR = os.path.join(ANALYSIS, "funcs")
os.makedirs(OUTDIR, exist_ok=True)

ADDRS = [int(a, 16) for a in sys.argv[1:]] or [
    0x14CB1, 0x150ED, 0x15106, 0x15119, 0x1513D, 0x15156, 0x15169,
    0x151A4, 0x151DE, 0x1536D, 0x15DAF, 0x1601A, 0x17FDE, 0x17FEC,
    0x18011, 0x18018, 0x1801F, 0x1A494, 0x1A4B7,
]

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

project, program = open_rotr2()
fm = program.getFunctionManager()
af = program.getAddressFactory().getDefaultAddressSpace()
di = DecompInterface()
di.openProgram(program)
monitor = ConsoleTaskMonitor()

seen = set()
for a in ADDRS:
    addr = af.getAddress(a)
    fn = fm.getFunctionContaining(addr)
    if fn is None:
        print("No function contains", hex(a))
        continue
    ep = fn.getEntryPoint().getOffset()
    if ep in seen:
        continue
    seen.add(ep)
    res = di.decompileFunction(fn, 180, monitor)
    code = res.getDecompiledFunction().getC() if res.decompileCompleted() else "// FAILED: " + res.getErrorMessage()
    path = os.path.join(OUTDIR, "fn_%x.c" % ep)
    open(path, "w").write(code)
    print("fn_%x.c (%s, size %s)" % (ep, fn.getName(), hex(fn.getBody().getNumAddresses())))
project.close()
