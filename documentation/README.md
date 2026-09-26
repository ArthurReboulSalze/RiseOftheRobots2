# Rise 2 research and port documentation

This index describes the experimental Rise 2: Resurrection port. **Current state:** local folder/ZIP/ISO/BIN-CUE imports; 28 or 30 selectable fighters depending on edition; MVS, CL2 and MRW conversions; three ANI movies converted; C++/SDL2 port in progress.

Director's Cut Disc 1 supplies the game, 105 ANI files and CD tracks 02–10. Disc 2 is optional. [Source imports](11_source_imports.md) explains the private data workflow; [the port handoff](10_port.md) covers implementation status and limits; [image decoding](07_image_decoding.md) records codec evidence. An imported profile's `EXTRACTED/index.html` is its local image gallery.

| Document | Subject |
|---|---|
| [01_overview.md](01_overview.md) | Goal, source editions and main findings |
| [02_work_plan.md](02_work_plan.md) | Work plan and phase status |
| [03_environment.md](03_environment.md) | Toolchain, private paths and commands |
| [04_journal.md](04_journal.md) | Chronological research and development log |
| [05_data_formats.md](05_data_formats.md) | Game data formats |
| [06_binary_analysis.md](06_binary_analysis.md) | Launcher and EXR reverse engineering |
| [07_image_decoding.md](07_image_decoding.md) | Validated image decoding |
| [08_engine.md](08_engine.md) | Engine loop, states, AI, collisions and damage |
| [09_remaining_formats.md](09_remaining_formats.md) | MRS/CHRSET, audio modes and additional ANI |
| [10_port.md](10_port.md) | Build, architecture, current behavior and remaining work |
| [11_source_imports.md](11_source_imports.md) | Private sources, edition handling, audio tracks and checks |
| [13_audio_investigation.md](13_audio_investigation.md) | Verified DOS quality settings, SOS parameters, modern conversion and MRS structure |

Working directories:

- `SRC/` — original game copies; **never modify**, excluded from Git.
- `LOCAL/<profile>/` — new private imports: `game/`, `music/`, `EXTRACTED/`, reports and optional analysis.
- `ANALYSIS/` — historical private analysis output, including Ghidra decompilation.
- `TOOLS/` — Python parsers and Ghidra scripts.
- `EXTRACTED/` — older private PNG/WAV/metadata export.
- `PORT/` — modern port source code.
- `Ghidra_Project/` — original private Ghidra project.
