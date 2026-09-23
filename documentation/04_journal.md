# 04 — Development journal

This is a chronological research log. Early interpretations are preserved here as history; documents 05, 07, 08, 09, and 10 describe the corrected current understanding. In particular, early claims that MVS held sound and MRW held video were disproved. The French originals remain in the local Git history; this public version records their findings in English.

## Session 20 — 2026-09-23: mounted drive and MIT license

The user mounted the Director's Cut Disc 1 data ISO on `I:\`, CDFS volume `RISE2_DC_D1`. `import_game.py --source I:\ --import-only` succeeded. All 1,122 game files copied from the drive had the same SHA-256 as the CUE/BIN import; 30 robots and 105 ANI files were found. There were no CDDA tracks in the mounted data-only ISO, so the importer selected digital music. This validates a virtual data disc, not CDDA ripping from a mixed-mode disc or a physical drive.

The user approved MIT. `LICENSE` was added with collective attribution to Rise 2 Port contributors. There is still no remote repository or GitHub upload.

## Session 19 — 2026-09-23: data ISO for virtual-drive testing

`TOOLS/cue_to_iso.py` extracted the MODE1 data track from the Director's Cut Disc 1 CUE/BIN into private `LOCAL/directors-cut-cd1-data.iso`. Size: 358,246,400 bytes; SHA-256: `621b90e46bffb387a15be92cdf971fe2ee0f9085a57766718509a482604c6f42`. Volume: `RISE2_DC_D1`. An ISO data track cannot contain the CDDA audio tracks. Reimport found 1,128 files on the volume, including 1,122 game files identical to the CUE/BIN source. The importer reports an unavailable audio TOC and keeps digital mode. Synthetic drive tests passed.

## Session 18 — 2026-09-23: local repository and private imports

Git was initialized on `main` with no remote. An allowlist-style `.gitignore` and index audit exclude original files, converted assets, executables, Ghidra projects, and decompiler exports. The CLI and GUI importer accept a game folder, ZIP, ISO9660, BIN/CUE, Windows CD drive, and separate numbered tracks 02–10 in a folder or ZIP. Profiles are isolated, hashed, and published only after successful conversion; different editions are not merged.

The original DOS folder plus nine MP3 tracks imported. ZIP and ISO variants yielded identical hashes across 996 game files. Without CD tracks, the importer chooses the original digital MGA–MGF music source; its playback is not yet in the port. Director's Cut Disc 1 alone yielded 1,122 game files, 30 robots, 105 retained ANI files, 71 MRW banks with 1,179 exported WAV files, and nine CDDA tracks. Disc 2 is bonus content, not required. The extra 102 ANI streams await decoding. The user chose this edition for future complete-content work.

The roster and gallery were updated for 30 robots. RBT2 is SHEEPMAN and RBT3 is BUNNYRABBIT; their own palettes suffice without matching RB4/AG assets. The LE loader was fixed for the final fixup entry, signed boundary sources, object-relative pages, and selectors without target offsets. The old copy has 10,589 fixups and entry `0x3ec2c`; Director's Cut has 10,634 and `0x3f19c`. Isolated private Ghidra projects found 537 and 430 functions, respectively. Existing analysis was left intact.

At that point, 22 import/LE tests, nine codec tests, Release build, `fighter_frames`, and a hidden SDL asset smoke check passed for both 28- and 30-robot profiles. A separate source-only checkout rebuilt and passed the same checks. GUI startup was checked, but full visual behavior and a physical CD drive were not tested.

## Session 17 — 2026-09-23: available ANI and frontend

`TOOLS/extract_ani.py` decoded the three original ANI streams exactly: LLOGO 39, END 61, and ENL 61 frames. LLOGO shows logos/legal material, not the complete film. `ENERGY.NFO` says cinematics were stripped from this distribution; `RQLINK.ANI` is missing. An earlier DOSBox error involving RQLINK alone did not prove that normal gameplay was impossible: the user reached a fight in the original game.

The C++ frontend began loading LLOGO, MAINSCR, VSFACE portraits, and per-fighter RBT/MVS/CL2 data. Video timing, font, and character-selection composition were provisional.

## Session 16 — 2026-09-23: correcting the port and resource mapping

The RBT atlas has an empty leading frame: MVS image `i` maps to atlas frame `i+1` and CL2 record `i`. MVS end markers no longer enter rendering; a completed move retains its last visible frame. Transition logs report source and target movement. `extract_mvs.py` now separates the four `+0x1c..+0x1f` bytes even for RBMG/RBMN and bounds each displacement table to its own sequence; all 30 JSON files were regenerated. Release build, `fighter_frames`, 8,640 MVS sequences, and old 28-robot atlas/CL2 alignment were checked. DOS timing, automatic MVS transitions, CL2 placement, and MRS pitch remained for comparison.

## Session 14bis — 2026-09-22: high-resolution decision

The user chose the game's native 640×400 high-resolution mode with RBT sprites as the port's primary mode. The window moved to 1280×800, with a provisional standing ground anchor near y=312. RB4 low-resolution data remains available for a possible original-style VGA mode.

## Session 14 — 2026-09-22: first animated fighter

MSVC 14.44, CMake, SDL2 2.30.11, SDL2_image 2.8.4, and nlohmann/json 3.11.3 compiled the first C++ skeleton. At this stage the prototype used a 320×200 logical surface and a 60 Hz loop, loaded atlas manifests and MVS JSON, applied signed movement deltas, and mapped temporary keyboard inputs to MVS transitions. The user visually confirmed the RB4A fighter's breathing idle sequence `0,0,1,1,2,2,3,3,2,2,1,1`. The high-resolution and timing changes above and in page 10 supersede this early prototype. A historical screenshot is at `documentation/captures/port_ossature_v1.png` in the private workspace.

## Session 13 — 2026-09-22: all MVS banks converted

`TOOLS/extract_mvs.py` converted 30 banks with 96 moves each: 2,880 movement descriptors. RBMG and RBMN use big-endian directory/descriptors/sequences and reversed magic. The transition table ends at a first word of `0xffff`; an empty table begins with it. Per-step displacement is signed 16-bit. RBT0 idle transitions include 0x10→7, 0x14→7, 0x12→7, 0x04→2, 0x02→3, 0x01→8, and 0x20→9. Input masks occupy six bits, but the game exposes ten physical controls (four directions, three punches, three kicks); mapping remains open. 2,537 of 2,880 moves have no direct transitions. MRS sequence reading and SOS pitch were also being traced.

## Sessions 12 and 12bis — 2026-09-22: audio confirmation and CD tracks

The user listened to representative extracted MRW samples and confirmed the effects sound correct. Nine provided CD-rip MP3 files were normalized to names `Piste 02.mp3` through `Piste 10.mp3`; track 01 is data. MSCDEX wrappers at `0x433e6/0x43410/0x43445/0x43564` are reached through the HMI SOS layer. Track 02 for title/menu and 03–10 for combat were only working hypotheses.

## Session 11 — 2026-09-22: locating the real samples

MRW, not MVS, is the sample-bank format. MRS drives sequences; SOUND.DAT configures audio hardware. The chain is `FUN_3be83` → `FUN_3a6b0` → `FUN_3a792` → `FUN_417c9` (SOS start call near `0x3a893`). MRW uses a u16 count and absolute (u32 offset, u32 size) pairs followed by unsigned 8-bit mono PCM, with `0x80` silence and an 11,025 Hz base rate. The original 69 banks yielded 1,142 WAV files; their spans cover about 99.8% of bank bytes. For R0 entry 0, offset `0x9a` and size 2,769 give a real waveform. Pitch adjusts playback rate as `(pitch_16_16 * 11025) >> 16`. `TOOLS/extract_mrw_audio.py` exports the banks.

## Sessions 9–10 — 2026-09-22: correcting MVS attribution

Playing presumed MVS audio at 11,025 Hz produced only noise; the user confirmed it by listening. Tracing the loader showed MVS is a 96-move-per-robot table, not an SOS codec. Each 32-byte descriptor contains three animation pointers, three displacement pointers, a transition pointer, and four control bytes. RBT0 idle at descriptor `0x190` and sequence `0xd90` matched the visible breathing animation. The early question “where are the samples?” was answered in session 11: MRW banks.

## Sessions 5–8bis — 2026-09-22: combat reverse engineering

The combat loop, player structure, MVS state machine, facing, physics, AI, collisions, damage, projectiles, particles, and background scrolling were mapped from `RISE2.EXR`. Important anchors include round controller `FUN_137ce`, frame update `FUN_2163a`, state dispatch `FUN_234fa`, physics `FUN_21baf`, facing `FUN_25615`, hit application `FUN_38b72`, AI `FUN_35a7c/35eb8`, and box comparison `FUN_3885e`. The player stride is 0x97, full health is 120, super meter caps at 24, and close-range threshold is 61 pixels. The exact formulas and uncertainties are in page 08.

`FUN_15d92` confirmed CL2 records: a 16-byte header, then three counts and 5/6/5-byte attack/body/single boxes. The original 28 robot CL2 files yield 9,204 frames, 1,119 attack boxes, 11,170 body boxes, and 7,091 single boxes. Box x/y scales are 4/2. `FUN_253a8` handles wall pushback, `FUN_244eb` three projectile slots per player, `FUN_22848` five particle slots, and `FUN_3a0d2` four background channels. Earlier claims that `FUN_25615` was a general frame step were corrected: it updates facing. The exact purpose of the third CL2 box group remains unresolved.

## Session 4 — 2026-09-22: first parsers, later corrected

The first `extract_data.py` parsers located AIP, STS, CTL, CL2, MRW, MVS, SOUND.DAT, `RISE2.CFG`, and high scores. AIP begins with two u32 counts, followed by 14-byte records and a dword table; RBT0 contains 29 movement records and 42 input dwords. CTL begins with four offsets and controls four arena channels. `RISE2.CFG` contains 46 u16 values including direction and attack scancodes; `HISCORE.DAT` has 12 eight-byte name/score records. The initial interpretations of MRW as video, MVS as audio, and SOUND.DAT as event mapping were wrong and are replaced by sessions 9–11 and document 09.

## Session 3 — 2026-09-22: image extraction breakthrough

Correcting GGF dimensions to 400×200 and 800×400 unlocked 185 images; A6K is 640×400. A6B/A6G/A6M are palette fragments. The ANL/ANR decoder extracted 126 banks and 20,969 frames; 9,232 high-resolution robot frames, 9,234 low-resolution frames, and 56 portraits were assembled into 139 atlas pages. `FUN_35247` reads AIP, not ANR; the sparse ANR format uses span records ending in `0xffff`. 456 sampled frames across all 126 banks matched the original x86 implementation, and nine codec regression tests passed. See page 07 for the exact algorithm and palette limits.

## Session 2bis — 2026-09-22: LZW and the resolved geometry block

Disassembly established LSB-first LZW, 9→12-bit codes, CLEAR `0x100`, EOI `0x101`, and a prefix/suffix dictionary, implemented in `TOOLS/lzw.py`. A compressed GGF holds a flag byte, 768 palette bytes, then indexed-pixel LZW data. An initial 320×250 interpretation produced scrambled images; later analysis proved linear 400×200 or 800×400 geometry and explained the blitter's 800-pixel cropping. The old DOSBox capture and candidate-size questions were resolved by session 3.

## Session 2 — 2026-09-22: catalogue and LE executable

A catalogue of 1,047 source files identified the 28 base-36 robot slots. `TOOLS/le2flat.py` decoded the Watcom LE executable: 24-byte object records, four-byte compact page map entries, per-page fixups, and additive linear type-7 relocations. `RISE2.EXR` was imported into a separate Ghidra project with 529 automatically identified functions and entry `0x3ec2c`. Loaders for robots, GGF/LZW, movement tables, palettes, and file I/O were located. Later LE import fixes are recorded in session 18 and the source-import guide.

## Session 1 — 2026-09-22: initial setup and DOS play

The initial DOS distributions were byte-identical, so the installed DOS folder became the early research reference. Ghidra headless and PyGhidra scripting worked, as did DOSBox 0.74-3. `RISE2.EXE` is a small 16-bit launcher; the actual game is the 32-bit DOS/4GW `RISE2.EXR`. The user launched the game normally and reached a LOADER-versus-DEADLIFT fight with animated background, HUD, and a French pause menu. A separate blind Enter-key experiment exited while trying to open absent `RQLINK.ANI`; it did not invalidate the successful normal run. The EXR contains a fallback path `C:\ACCLAIM\RISE2`, and its on-disk hash did not change after play. The initial reverse-engineering and format-extraction plan began here.
