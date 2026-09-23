# 06 — Binary analysis

The addresses in this document refer to the **older DOS copy** unless noted otherwise. Director's Cut has a different EXR build.

## `RISE2.EXE` — launcher (19,724 bytes)

This is a 16-bit Watcom C/C++ DOS stub, not the game. Its reconstructed flow (`ANALYSIS/launcher_decompiled.c`, `main = FUN_1000_0061`) is:

1. Initialize the Watcom CRT and inspect the environment.
2. Locate `dos4gw.exe`, apparently considering `DOS4GPATH`. Strings include `DOS4GPATH`, `dos4gw.exe` and `rise2\dos4gw.exe`.
3. Open `RISE2\RISE2.EXR`, first through a relative path such as `.\rise2\rise2.exr`.
4. Print "Patching main executable from <path>\RISE2.EXR" as a status message.
5. Execute `dos4gw.exe` with `RISE2.EXR`. Error strings include "Stub exec failed" and "Bad format on exec."

At EXR file offset **0x52421** is the 16-byte installation path `C:\ACCLAIM\RISE2`, surrounded by CR/LF/NUL bytes, six FF bytes, two zero bytes and counters. It is a plausible patch target. After a full DOSBox run, the on-disk EXR had the same MD5 hash. The patch may occur only in memory or do nothing when the default path already works; this remains to be checked.

The EXR appears to resolve game data through `MIRAGEGAME`, relative `RISE2`/`\RISE2` paths and an embedded fallback path. Referenced filenames include `RISE2.CFG`, `CHRSET3.DAT`, `TESTD.RAW` and `RQLINK.ANI`. RQLINK is absent from the older stripped copy but present in Director's Cut.

## `RISE2.EXR` — main program (421,979 bytes in older copy)

- **32-bit DOS/4GW LE (Linear Executable)**, with `e_lfanew = 0xAD8` and the LE signature at that offset.
- Ghidra 12.1.4 uses our `TOOLS/le2flat.py` loader. The actual image base is **0x10000**, rather than the initially assumed 0x400000.
- Early string scans found `File missing`, `MIRAGEGAME`, `RISE2`, `\RISE2`, `Please run the…`, `CHRSET3.DAT`, `wb`, `RISE2.CFG` and `TESTD.RAW`.

`DOS4GW.EXE` (265,420 bytes) is the Rational DOS/4GW extender invoked by the launcher.

## Private analysis artifacts

- `ANALYSIS/launcher_decompiled.c`: launcher decompilation (137 functions).
- `ANALYSIS/launcher_disasm.txt` and `launcher_functions.txt`/`launcher_strings.txt`.
- `TOOLS/export_launcher.py` and `TOOLS/export_listing.py`: reusable PyGhidra scripts.

### Older EXR import into Ghidra (September 22, 2026)

- The first `TOOLS/le2flat.py` pass generated `ANALYSIS/RISE2_flat.bin` (406,784 bytes, linear 0x10000–0x73500). Its fixup validation was incomplete; use the corrected loader below for new analysis.
- `TOOLS/import_exr_ghidra.py` imported the flattened image, prefixed with 64 KiB of zero bytes, as `RISE2_EXR` in private `ANALYSIS/exr_proj/rotr2_exr`. This makes **file offset equal linear address**.
- Initial auto-analysis found **529 functions** and decompiled the Watcom CRT entry at 0x3EC2C into `ANALYSIS/exr_entry.c`.
- `TOOLS/ghidra_session.py` provides `open_rotr2()`. `TOOLS/exr_decompile_at.py <hex-address>` writes a selected function to private `ANALYSIS/funcs/fn_<address>.c`.

### Corrected loader and isolated editions (September 23, 2026)

`TOOLS/le_codec.py` reads **page_count + 1** fixup offsets, signed source offsets and page positions relative to each object. Selector records (type 2) have no target offset; they are annotated because DOS assigns selector values at runtime. Type 7 is an absolute 32-bit address, while type 8 is a relative 32-bit displacement. Unsupported records or invalid bounds fail the import.

| Source | Pages | Fixup records | Unique patched addresses | Selector annotations | Entry |
|---|---:|---:|---:|---:|---|
| Older copy | 80 | 10,589 | 10,581 | 2 | 0x3EC2C |
| Director's Cut Disc 1 | 81 | 10,634 | 10,624 | 2 | 0x3F19C |

Duplicate fixups at page boundaries are checked. Five synthetic tests cover them and truncated records. `TOOLS/prepare_analysis.py --profile LOCAL/<profile>` writes into an isolated private profile, preserving the historical Ghidra project. Addresses in documents 06–09 still refer to the older EXR until Director's Cut receives its own detailed map. Format reference: [Open Watcom headers](https://github.com/open-watcom/open-watcom-v2/blob/master/bld/watcom/h/exeflat.h).

## Key identified functions (older copy, Ghidra base 0x10000)

| Address | Role |
|---|---|
| `FUN_00014c04` | Per-robot loading chain: GGF → PAL → MVS/STS → AN → CL2 |
| `FUN_0003c731` | GGF LZW decompression and horizontally cropped linear copy |
| `FUN_0003c9d0` / `FUN_0003caac` | LZW initialization (9→12 bits, CLEAR 0x100, EOI 0x101) / LSB-first bit reader |
| `FUN_00035247` | **AIP** parser: N1 14-byte records + N2 dwords; `RBTn.AIP` string at 0x62ad8 |
| `FUN_0001a0fc` / `FUN_0001a23f` | ANL/ANR loaders: u16 count and u32 offsets, complete or selective loading |
| `FUN_0001c819` | Sprite blitter; 0x1c9d6–0x1ca6d decode four span types |
| `FUN_00020dda` | Adds 70 to ANR color indices below 70 for player 2 |
| `FUN_00023e97` | Per-robot MVS/STS parser with 96 self-relative pointers |
| `FUN_0001524f` | `?.AN` loader through `FUN_0001a23f` |
| `FUN_0001a473` | `?.PAL` loader: two palettes per robot through `FUN_0001b3f4` |
| `FUN_0001b304` / `FUN_0001b39e` / `FUN_0001b3e6` | DOS open, chunk read, close (INT 21h) |
| `FUN_000121c3` | Whole-file read into memory starting at `DAT_00065be0` |
| `FUN_0001473e` | Input/quit check during loading |
| `FUN_0001b077` | Loading-screen update |
| `DAT_00066354` | Current robot index (base 36); `DAT_0006639c` mode; robot IDs 0x13/0x16/0x18 treated by `FUN_0001aac2` |

`ANALYSIS/exr_strings.txt` records 106 Ghidra-defined strings and cross-references; more raw strings still need classification. Image decoding and verification are in [document 07](07_image_decoding.md). Private assembly exports (`asm_fn_1c819.txt`, `asm_fn_1d387.txt`, `asm_fn_20dda.txt`, `asm_fn_1a0fc.txt`, `asm_fn_1a23f.txt`, `asm_fn_1a473.txt`, `asm_fn_35247.txt`) preserve the instructions used. `TOOLS/verify_x86_images.py` runs them under Unicorn as an independent reference for the Python decoder.
