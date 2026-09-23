# Import de l'image plate RISE2 (LE aplatie, base déclarée 0x10000) dans Ghidra.
# Le fichier est préfixé de 64 Ko de zéros => offset fichier = adresse linéaire.
# Projet : ANALYSIS/exr_proj (projet privé, GUI fermée).
import os, struct, json

from project_paths import ROOT as PROJECT_ROOT, SOURCE, ANALYSIS
ROOT = str(PROJECT_ROOT)

FLAT = str(ANALYSIS / "RISE2_flat.bin")
PADDED = str(ANALYSIS / "RISE2_flat_padded.bin")
PROJ = os.path.join(ROOT, r"ANALYSIS")
metadata = json.loads((ANALYSIS / "RISE2_le_metadata.json").read_text(encoding="utf-8"))
ENTRY = metadata["entry"]
if metadata["base"] != 0x10000:
    raise ValueError("Base LE non prise en charge par ce projet Ghidra")

img = open(FLAT, 'rb').read()
pad = b'\x00' * 0x10000
open(PADDED, 'wb').write(pad + img)

import pyghidra
pyghidra.start()
from ghidra.program.model.address import AddressSet
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

with pyghidra.open_program(
    PADDED,
    project_location=os.path.join(ANALYSIS, "exr_proj"),
    project_name="rotr2_exr",
    analyze=True,
    language="x86:LE:32:default",
    program_name="RISE2_EXR",
) as flat:
    program = flat.getCurrentProgram()
    af = program.getAddressFactory().getDefaultAddressSpace()
    entry = af.getAddress(ENTRY)
    # point d'entrée : désassembler + créer la fonction
    flat.disassemble(entry)
    f = flat.createFunction(entry, "entry")
    print("entry function:", f)

    # décompiler l'entrée pour vérifier que le code est cohérent
    di = DecompInterface()
    di.openProgram(program)
    fm = program.getFunctionManager()
    res = di.decompileFunction(f, 120, ConsoleTaskMonitor())
    out = res.getDecompiledFunction().getC() if res.decompileCompleted() else "// FAILED: " + res.getErrorMessage()
    open(os.path.join(ANALYSIS, "exr_entry.c"), "w").write(out)
    print("=== entry (extrait) ===")
    print(out[:1500])
    print("fonctions trouvées:", fm.getFunctionCount() if (fm := program.getFunctionManager()) else "?")
