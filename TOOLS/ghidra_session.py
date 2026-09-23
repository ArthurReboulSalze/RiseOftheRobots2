# Session PyGhidra réutilisable : ouverture du programme RISE2_EXR stocké dans ANALYSIS/exr_proj.
# Import: from ghidra_session import open_rotr2  ; puis work(program, flat)
import os
from project_paths import ANALYSIS

PROJ_LOC = str(ANALYSIS / "exr_proj/rotr2_exr")
PROJ_NAME = "rotr2_exr"
PROGRAM_PATH = "/RISE2_EXR"

import pyghidra
pyghidra.start()
from ghidra.base.project import GhidraProject


def open_rotr2():
    project = GhidraProject.openProject(PROJ_LOC, PROJ_NAME, True)
    program = project.openProgram("/", "RISE2_EXR", True)
    return project, program


if __name__ == "__main__":
    project, program = open_rotr2()
    print("OK:", program.getName(), "fonctions:", program.getFunctionManager().getFunctionCount())
    project.close()