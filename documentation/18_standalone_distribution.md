# 18 — Standalone Windows distribution

The public repository contains the port and importer source, not game data. The player-facing goal is a single visible `Rise2.exe` entry point, with private runtime files inside its distribution folder. The prebuilt C++/SDL2 game remains a separate internal executable; it is never compiled on the player's computer. The Python importer is bundled with its interpreter and libraries. Ghidra is a research tool and is not part of the player workflow.

## Launch and storage

`TOOLS/launcher.py` opens the import window when no converted profile exists. The player chooses their own folder, ZIP, ISO, CUE/BIN or CD, plus optional numbered music tracks. `run_import` checks and converts the files under a temporary directory and publishes the profile only after success. On completion the launcher starts `runtime/rotr2.exe --assets <profile>/EXTRACTED`. On later launches it offers the saved profiles and an option to import another copy.

The default profile location is `%LOCALAPPDATA%/Rise2ResurrectionPort/profiles/<name>`. It is outside the program folder, so replacing a package leaves imported data and per-profile settings intact. `RISE2_USER_DATA` can override the root for tests or portable setups. No network download or DOS executable execution is involved.

The converters expose argument-taking `main(argv)` functions and run directly inside the importer rather than launching a system Python process. The ANI converter still uses a process pool; the frozen launcher calls `multiprocessing.freeze_support()` before processing command-line arguments. The bundled image-gallery template is read from the package's internal resource directory.

## Build

On a Windows development machine, install `requirements.txt` and `requirements-build.txt`, bootstrap and build the native Release target, then run:

```powershell
.\.venv\Scripts\python.exe TOOLS/build_distribution.py
.\LOCAL\distribution\Rise2\Rise2.exe --verify-package
```

The build uses PyInstaller's one-folder mode and copies the Release executable, SDL DLLs, Python and dependency license notices, and project license into `LOCAL/distribution/Rise2/`. The project PNG is converted into the Windows executable and Tk icons, and shipped beside the native executable for the SDL window icon. This folder is ignored by Git and contains no original game files. The package manifest records hashes of the launcher and native runtime files. A single physical `.exe` is not required for the one-entry player workflow; the internal Python and SDL files remain beside it.

## Validation and limits

Synthetic importer and launcher tests run without original data. The frozen package was copied into a different private folder and run from `C:\Windows` with `PATH` limited to Windows system directories. Both the first-run import window and the existing-profile selector initialized. A complete DOS folder import produced 996 game files, 28 robots and three ANI files; a complete Director's Cut Disc 1 CUE/BIN import produced 1,122 game files, 30 robots, 105 ANI files and CD tracks 02–10. Both profiles passed `rotr2_asset_smoke`. The packaged launcher's `--play-profile` path started the internal SDL game with each profile; both remained running through a short check. Director's Cut loaded menu music and eight combat tracks. These checks establish local package behavior, not operation on an untouched Windows installation.

Folder, ZIP, ISO and CUE handling share the existing importer. The frozen package also imported the owned 996-file ZIP and ISO through its hidden `--import-only` check; these inspection profiles are deliberately not playable. Synthetic tests cover malformed archives and source identification. The release ZIP was checked for CRC errors and original-game file types, extracted into a fresh private directory, then checked with `--verify-package`, `--gui-smoke`, profile discovery and a live DOS-profile game launch. The real physical-CD audio path still needs optical hardware testing. Optional FLC bonus movies require a separate FFmpeg installation; the normal game import does not.

The current package is a Windows x64 alpha. Operation on a clean Windows installation, physical CD audio, and antivirus behavior still need independent testing. The port's remaining gameplay, campaign and MRS digital-music limitations are unchanged by packaging.
