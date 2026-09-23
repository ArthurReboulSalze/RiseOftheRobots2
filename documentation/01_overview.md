# 01 — Project overview

## Goal

Port **Rise 2: Resurrection** (1996, Mirage Technologies / Acclaim Entertainment), a 2D DOS fighting game, to modern platforms, with Windows as the initial target.

## Game structure

- Two fighters appear in a **2D** side-view arena.
- Animated backgrounds use pre-rendered loops, not real-time 3D scenery.
- The fighters are pre-rendered 3D models represented in the game as 2D animation frames.
- The port therefore consumes image atlases and animation scripts; it does not need a 3D engine.

## Local source inventory (`SRC/`, excluded from Git)

| Source | Role |
|---|---|
| `DOS_version_install_cracked/` | Older complete DOS copy, byte-identical to the older extracted ISO. It has 28 robots and stripped long cinematics. Historical executable addresses refer to this copy. |
| `ISO_version_install/` | Extracted older ISO, used to corroborate the DOS folder. |
| `DOS_Installed_Files/ACCLAIM/RISE2/` | Minimal installed state: `RISE2.CFG` (controls/options) and `HISCORE.DAT` (scores). |
| `Rise 2 Directors Cut/Disc 1/` | Game BIN/CUE: 1,122 files under RISE2, 30 robots, 105 ANI files and nine audio tracks. Chosen source for future extraction. |
| `Rise 2 Directors Cut/Disc 2/` | Bonus FLC/GIF/WAV files and documents; unnecessary for the game port. |

### Older DOS copy: executable structure

- `RISE2.EXE` (19,724 bytes) is a **16-bit Watcom launcher**. It locates `dos4gw.exe`, patches `RISE2.EXR` and starts it under DOS/4GW. It is not the main game.
- `RISE2/RISE2.EXR` (421,979 bytes) is the main **32-bit LE (Linear Executable)** DOS/4GW program. Its LE signature is at the MZ header's `e_lfanew = 0xAD8`.
- `RISE2/DOS4GW.EXE` (about 265 KB) is the Rational DOS extender.
- See [document 05](05_data_formats.md) for data files.

### Findings that affect analysis

1. The two **older** distributions are identical. Director's Cut is a different build; do not combine its banks or executable addresses with the older copy. See document 11.
2. The launcher patches `RISE2.EXR` before starting it. Strings include `Patching main executable from C:\ACCLAIM\RISE2\RISE2.EXR` and relative lookup paths. The exact in-memory patch behavior remains a separate analysis question.
3. Ghidra 12.1.4 did not provide a usable LE/LX loader in this environment. `TOOLS/le2flat.py` and PyGhidra import the flattened image at its actual base, **0x10000**. Image routines were also checked against x86 execution; see documents 06 and 07.
4. `RISE2/ERRORS.TXT` contains readable, numbered messages such as "Error reading GGF," which help locate loader routines.

## Current extracted images

The older copy yielded 185 readable GGF images and 28 robots in two resolutions, with 56 portraits: 18,522 frames on 139 atlas pages. The local gallery is `EXTRACTED/index.html`. Director's Cut adds two robots. See [image decoding](07_image_decoding.md) for validation and remaining limits.
