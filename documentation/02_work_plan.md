# 02 — Work plan and progress

This page tracks the project's phases. Counts for the older copy and Director's Cut are kept separate.

| Phase | Status | Evidence or deliverable |
|---|---|---|
| 0. Environment, documentation, DOSBox boot test | Complete (2026-09-22) | Project layout, Ghidra headless pipeline and a DOSBox fight reached |
| 1. Data formats and asset extraction | In progress | GGF, sprites, MVS, MRW, CL2 and three ANI decoded. Director's Cut Disc 1 chosen for the rest: 30 robots, 1,179 audio samples and 105 ANI, of which 102 remain to convert |
| 2. Reverse engineer `RISE2.EXR` | In progress | LE import, image decoders and x86 checks; game rules still under analysis |
| 3. Choose port architecture | Complete | C++17/SDL2 reimplementation |
| 4. Develop the port | In progress | Logos, title, 28- or 30-robot selection, initial 640×400 combat, HUD and MVS/CL2 integration |
| 5. Validate and package | In progress | Private import pipeline and local Git repository; no GitHub publication before the remaining checks |

## Phase 0 — Environment

- [x] Create `TOOLS/`, `ANALYSIS/`, `EXTRACTED/`, `PORT/` and `documentation/`.
- [x] Install PyGhidra and validate headless import for the launcher.
- [x] Inspect the launcher's patch target at file offset 0x52421. A DOSBox run left the EXR file unchanged on disk, consistent with an in-memory or idempotent patch.
- [x] Boot the original game in DOSBox and reach an active LOADER vs DEADLIFT fight.

## Phase 1 — Data formats

Validated on the older copy unless noted otherwise:

- [x] GGF geometry: 400×200 and 800×400; A6K is 640×400. Export 185 images and report three invalid fragments.
- [x] Decode ANL/ANR offsets and pixel spans: 126 banks and 20,969 frames checked.
- [x] Export 28 robots in two resolutions plus portraits: 18,522 frames on 139 pages.
- [x] Preserve transparency, palette indices and sprite positions.
- [x] Build a local gallery and compare codec output with x86 instructions.
- [x] Map MVS image `i` to robot atlas frame `i+1` and CL2 record `i`.
- [x] Decode LLOGO, END and ENL: 161 images. The older distribution says its long cinematics were stripped (`ENERGY.NFO`).
- [x] Import Director's Cut Disc 1 with 30 robots, 105 ANI files and CD audio.
- [ ] Decode the other 102 ANI files and their timing.
- [ ] Complete AIP/MVS/STS timing and transitions, MRS sequences, contextual ANR banks, CTL scripts and CHRSET fonts.

Available parsers cover `.GGF`, `.PAL`, `.ANL`/`.ANR`, `.MVS`, `.MRW` and `.CL2`. Remaining work includes `.MRS`, `.CTL`, `.STS`, several `.DAT` files and `RISE2.CFG`. Binary loader routines provide a reference for interpreting these formats. Exports go into private `EXTRACTED/` as PNG, WAV and JSON.

## Phase 2 — Binary analysis

1. The Python LE reader handles headers, object/page tables, fixups and entry points. Corrections for page boundaries and selectors are described in document 06.
2. Ghidra imports the flattened x86:32 image at **0x10000** with internal fixups applied. Director's Cut receives its own isolated analysis project.
3. Separate Watcom and DOS/4GW runtime code from game logic to reduce decompiler noise.
4. Continue mapping the game loop, input, AI, fighter states, attacks, damage, VESA 640×400 rendering, audio and file loaders. `ERRORS.TXT` is a useful anchor.

## Phase 3 — Architecture

| Option | Description | Benefit | Cost |
|---|---|---|---|
| A, chosen | Modern C++/SDL2 engine using extracted assets and reconstructed rules | Maintainable native port | Significant game-logic work |
| B | Recompile decompiled code with video/audio/OS shims | Closer to the original program | Very large effort and difficult maintenance |
| C | Preconfigured DOSBox wrapper | Fast to ship | Not a native port |

Windows is the first target and the logical resolution is 640×400. Animation, collision and control fidelity still need comparison with the DOS game.

## Phases 4–5 — Port and validation

The implementation sequence is: engine skeleton → asset pipeline → title and selection → arena/HUD/rounds → combat rules → AI → audio. Compare the result with DOSBox captures, complete the feature checklist and package a release.

The immediate priorities are to finish the source-import checks in document 11, then decode Director's Cut Disc 1's additional movies and content. Do not publish to GitHub before validation, especially the real physical-CD path. MRS and CD music playback and the remaining gameplay are described in document 10.
