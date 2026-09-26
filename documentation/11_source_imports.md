# 11 — Source imports and repository preparation

Status: September 26, 2026. Code and documentation are published on GitHub. Game files, conversions and decompiler output are excluded. The root README gives public setup instructions.

## Editions and scope

| Verified source | Files in RISE2 | RBT robots | ANI files | MRW banks / samples |
|---|---:|---:|---:|---:|
| Older DOS folder | 996 | 28 | 3 | 69 / 1,142 |
| Director's Cut, Disc 1 | 1,122 | 30 | 105 | 71 / 1,179 |

**Director's Cut needs only Disc 1:** the game and CD audio tracks 02–10 are on this disc. Disc 2 has 275 bonus files (GIF, FLC, WAV, documents and utilities) but no game banks. Importing both discs was compared with importing Disc 1 alone: every game file and main audio track had the same SHA-256 hash; bonus files stayed separate.

Director's Cut is the chosen source for **future extraction**, with all available game content retained. RBT2 and RBT3 add SHEEPMAN and BUNNYRABBIT. All 30 portraits and high-resolution robot banks are supported. These two extra robots have no RB4 banks or dedicated AG backgrounds on this disc; their own palettes cover the color indices used by their sprites.

All 105 ANI files are imported. LLOGO, END and ENL currently become 161 exported frames; the other **102 ANI files** await further decoding and integration. A successful import does not mean every cinematic or game system is playable yet.

## Import pipeline

1. `TOOLS/import_gui.py` or `TOOLS/import_game.py` accepts a folder, ZIP, ISO9660, CUE/BIN or Windows optical drive. The game folder is identified by RBT0.ANR and RBT0.MVS.
2. Archives and discs are read into a temporary directory. A CUE/BIN data track is read directly, without mounting or executing it. MODE1/2048, MODE1/2352 and MODE2/2352 Form 1 are supported. INDEX 00, INDEX 01 and synthetic PREGAP positions are handled separately. `TOOLS/cue_to_iso.py` can also copy that data track into an ISO9660 file for mounting in Windows; this ISO has no audio tracks.
3. The game folder is copied into `game/` with uppercase names and SHA-256 hashes. Different editions are never merged. Originals are not modified. A minimal installed folder containing only configuration is insufficient; ambiguous names, escaping paths and links are rejected.
4. Music is copied into `music/` and indexed. A folder or ZIP given with `--music` takes priority. Otherwise, the soundtrack from the disc containing the game is selected. Audio on a bonus disc does not replace the main soundtrack.
5. The GGF, ANR, MVS, CL2, MRW and known-ANI converters, UI font builder and gallery builder produce `EXTRACTED/`. Atlas/MVS/CL2 relationships are checked for every robot.
6. The profile and report appear only after the pipeline succeeds. An existing profile is never overwritten. `--import-only` skips asset conversion while checking the files required by the port.

The converter reports three incomplete GGF fragments, A6B/A6G/A6M, without failing the import: these fragments do not contain a complete usable image. Other source files remain in the private profile even if their formats have not yet been interpreted.

## Audio sources

CD tracks keep numbers **02–10**. CUE/BIN audio is exported as 44,100 Hz, stereo, 16-bit PCM WAV, without lossy transcoding. The inspected images store CDDA samples in little-endian order. Synthetic CUE gaps do not shift offsets within the BIN; stored INDEX 00 sectors are excluded from the preceding track.

Separately supplied music can be WAV, MP3, FLAC, OGG or M4A. Recognized names include `02.wav`, `Piste 02.mp3` and `Track 02.flac`. Duplicate or ambiguous track numbers fail the import. A data-only ISO contains no CD audio. Compressed files are preserved as supplied and checked by signature; the importer does not fully decode and validate every compressed format.

The game's digital music uses **MGA–MGF MRS/MRW pairs**: sequences and samples, rather than standard MIDI files. The manual and `FUN_150b4` confirm this mode (documents 08 and 09). `auto` selects CD audio, then digital music, ambience and silence according to availability. Explicit modes are `cd`, `digital`, `effects` and `off`; asking for an unavailable mode fails.

**The chosen mode is saved in settings.json. The port currently plays imported CD tracks for its menu and combat screens; the MRS digital-music sequencer is not yet implemented.** Digital banks are retained even when CD audio is chosen.

## Optional binary analysis

`prepare_analysis.py --profile LOCAL/<profile>` reconstructs the LE image and writes a report inside `LOCAL/<profile>/ANALYSIS/`. With `--ghidra` and `--java-home`, it creates an isolated Ghidra project (pyghidra is needed in the selected Python environment). The historical Ghidra project is not replaced. To reuse the analysis scripts:

```powershell
$env:RISE2_ANALYSIS = "$PWD/LOCAL/my-game/ANALYSIS"
python TOOLS/exr_decompile_at.py <address-from-this-edition-in-hex>
```

The LE loader handles fixups spanning page boundaries, the final page and selector fixups with no target offset; see document 06. Preparation and Ghidra import were run for both editions. Addresses from the older DOS executable cannot be applied blindly to Director's Cut.

## Verification performed locally

| Case | Result |
|---|---|
| DOS folder plus nine separate MP3s | Full import and conversions; original music retained without transcoding |
| ZIP built from that folder | All 996 files match the folder by SHA-256; digital music selected without CD audio |
| ISO9660 built from that folder | The same 996-file comparison; digital music selected without CD audio |
| Director's Cut Disc 1 CUE/BIN | Full import and conversions; 30 robots, 105 ANI retained, nine WAV CD tracks |
| Disc 1 plus Disc 2 CUE/BIN | Same game and main music as Disc 1 alone; bonuses kept separately |
| Data-only ISO copied from Director's Cut CUE/BIN | 358,246,400 bytes; volume RISE2_DC_D1; 1,128 files, including 1,122 game files matching the CUE/BIN import by SHA-256 |
| Simulated data-only virtual CD without an audio TOC | Game data imported, digital music selected, unavailable CDDA reported |
| ISO mounted by Windows as `I:\` (CDFS) | Drive import succeeded; 1,122 game files match CUE/BIN hashes; 30 robots and 105 ANI detected; no CDDA tracks in the ISO, so digital music selected |
| Synthetic Python tests | Import/LE tests and nine image-codec tests pass without original game bytes |
| Release build and CTest | Build and `fighter_frames` test pass |
| C++ asset loaders with real private profiles | Logos, title, portraits and banks loaded for all 28 and then all 30 robots |
| Checkout containing only Git-index files | Pinned dependencies downloaded and verified; build and tests passed without bundled game data |
| Real physical CD | **Not yet tested:** no optical drive was available on the test machine |

Synthetic fixtures contain no original game data. Test profiles and reports stay in `LOCAL/`. The ISO mounted by the user confirms CDFS drive detection and file copying through the virtual drive. It cannot validate CDDA from a mixed-mode disc or a real optical reader. Local validation used Python 3.14.5; the published CI matrix for 3.12 and 3.14 passes, including a clean Release build. The Tkinter window initialized without error, though this does not replace user testing of every button.

The physical-drive path uses Windows TOC/CDDA read calls, checks read lengths and retries errors up to three times. Simulated tests do not replace real disc and hardware testing; this is not a secure ripper with jitter correction or AccurateRip matching. To test CDDA in a virtual drive, mount the original CUE/BIN in a drive that exposes its audio tracks. A data-only ISO is sufficient to test detection and data copying; a missing audio TOC does not block that import and is reported.

Verification commands:

```powershell
.\.venv\Scripts\python.exe -m unittest discover -s TOOLS/tests -v
.\.venv\Scripts\python.exe TOOLS/test_image_codecs.py
ctest --test-dir PORT/build -C Release --output-on-failure
.\PORT\build\Release\rotr2_asset_smoke.exe LOCAL/my-game/EXTRACTED
python TOOLS/audit_repo.py --staged
```

`rotr2_asset_smoke` uses a hidden SDL window to load the logos, menus, portraits and every robot bank through the actual C++ loaders. It checks the full roster without replacing visual comparison with the DOS game. CI uses synthetic fixtures and builds the port; it downloads no game data.

## What belongs in the public repository

`.gitignore` denies new root folders by default and allows only known code, test, HTML-template and documentation paths plus the project banner. SRC, EXTRACTED, LOCAL, ANALYSIS, Ghidra projects, captures, builds and downloaded dependencies are excluded. The gallery template is versioned; the rendered gallery containing game images is not.

`audit_repo.py` checks the complete Git index for allowed paths, UTF-8 text, binary files and some secret/embedded-media patterns. The sole binary exception is the project banner, allowed only at its exact SHA-256. This complements manual inspection; it is not a universal secret detector or legal analysis. Windows dependencies are fetched from their official projects at pinned versions with SHA-256 verification. The importer does not execute the DOS binaries. Decompilation files remain private.

Our code and documentation use the [MIT license](../LICENSE), with the collective notice "Rise 2 Port contributors." The license does not cover original game data. The public repository contains the reviewed code/documentation index. Real physical-CD testing remains pending; the tested CUE/BIN and virtual-drive imports are documented above.

Technical references: [pycdlib](https://clalancette.github.io/pycdlib/pycdlib-api.html), [Windows RAW_READ_INFO](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddcdrm/ns-ntddcdrm-__raw_read_info), [Windows CDDA read](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddcdrm/ni-ntddcdrm-ioctl_cdrom_raw_read).
