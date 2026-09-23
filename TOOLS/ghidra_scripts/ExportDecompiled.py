# Export function list, decompiled C, strings, and cross-references.
# Usage headless : -postScript ExportDecompiled.py <output_dir>
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
from ghidra.program.model.data import StringDataType
import os

OUT_DIR = getScriptArgs()[0] if getScriptArgs() else os.path.abspath("ANALYSIS")
os.makedirs(OUT_DIR, exist_ok=True)

program = currentProgram
fm = program.getFunctionManager()
listing = program.getListing()
monitor = ConsoleTaskMonitor()

# --- Function list and decompiled C ---
iface = DecompInterface()
iface.openProgram(program)

func_list_path = os.path.join(OUT_DIR, "functions.txt")
decomp_path = os.path.join(OUT_DIR, "decompiled.c")
with open(func_list_path, "w") as fl, open(decomp_path, "w") as dc:
    for f in fm.getFunctions(True):
        fl.write("%s  %s  (size=%d)\n" % (f.getEntryPoint(), f.getName(), f.getBody().getNumAddresses()))
        res = iface.decompileFunction(f, 60, monitor)
        dc.write("// ===== %s @ %s =====\n" % (f.getName(), f.getEntryPoint()))
        if res.decompileCompleted():
            dc.write(res.getDecompiledFunction().getC())
        else:
            dc.write("// decompilation failed: %s\n" % res.getErrorMessage())
        dc.write("\n\n")

# --- Defined strings and cross-references ---
strings_path = os.path.join(OUT_DIR, "strings.txt")
with open(strings_path, "w") as sf:
    for d in listing.getDefinedData(True):
        if isinstance(d.getDataType(), StringDataType) or "char" in d.getDataType().getName():
            val = d.getValue()
            if val is None or len(str(val)) < 4:
                continue
            refs = d.getReferenceIteratorTo() if hasattr(d, "getReferenceIteratorTo") else None
            ref_strs = []
            try:
                for r in program.getReferenceManager().getReferencesTo(d.getAddress()):
                    ref_strs.append(str(r.getFromAddress()))
            except Exception:
                pass
            sf.write("%s  len=%-4s  refs=[%s]  %r\n" % (d.getAddress(), d.getLength(), ",".join(ref_strs[:6]), str(val)))

print("Export done -> %s, %s, %s" % (func_list_path, decomp_path, strings_path))
