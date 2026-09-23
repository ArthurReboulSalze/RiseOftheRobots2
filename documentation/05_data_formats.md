# 05 — Game data formats

The older DOS copy at `SRC/DOS_version_install_cracked/RISE2/` was the original reference. Its raw catalog is private (`ANALYSIS/catalog.md` and `catalog.csv`). Director's Cut is a separate edition; see document 11.

Earlier guesses about GGF geometry and the ANR parser were wrong. [Document 07](07_image_decoding.md) contains the corrected image layouts and evidence.

## Validated image formats (older copy)

| Extension | Count | Structure and status |
|---|---:|---|
| `.GGF` | 188 | u8 flag, 768-byte palette when nonzero, LZW stream and zero padding. 185 images: 91 at 400×200, 93 at 800×400, A6K at 640×400. A6B/A6G/A6M are incomplete fragments. |
| `.ANL` | 126 | u16 frame count followed by that many little-endian u32 offsets into the ANR file. |
| `.ANR` | 126 | Horizontal pixel spans with absolute or compact relative coordinates; u16 0xffff ends each frame. 20,969 frames validated. |
| `.PAL` | 32 | VGA 6-bit RGB. R?.PAL has 80 entries (70 loaded per player); VSFACE.PAL/ROBOTS.PAL have 28 blocks of 70 colors; CHIPS.PAL has 240 colors; EXTRA.PAL needs more analysis. |

The older copy's 28 character slots use `ABCDEFGHIJKLMNOPQRSTUVWXYZ01`. The full alphabet embedded in its EXR at 0x5087c is `ABCDEFGHIJKLMNOPQRSTUVWXYZ012345`.

ANL/ANR bank families:

- `RBT?`: 640×400 fighters; `RB4?`: 320×200 versions.
- `VSFACE` and `V4FACE`: high- and low-resolution portraits.
- `BGANIM?` and `BGANI4?`: animated background elements using the same span codec.
- `CHIPS/CHIPL`, `CURSOR/CURSOL`, `EXTRA/EXTR4`, `HANDICAP/HANDICAL`, `POPSCRNS/POPSCRNL` and `RBAN/RBAL`: other banks. Their spans are decoded; contextual palettes and precise roles remain under investigation.

`TOOLS/extract_anr.py` exports 56 fighter banks plus two portrait banks from the older copy, totaling 18,522 frames. Color atlases, palette-index pages and positions are in private `EXTRACTED/sprites/`. Transparency means **no span was drawn at that pixel**; it is not represented by a special palette index.

## AIP — corrected attribution

`FUN_35247` reads `RBTn.AIP` (filename string at 0x62ad8), not ANR. The older copy's 28 AIP files contain:

```text
u32 N1, u32 N2
N1 × 14-byte record
N2 × 4-byte record
```

A 14-byte record comprises one u32, three u8 values, three u16 values and one u8; the game expands it to 20 bytes in memory. `RBT0.AIP` has N1=29 and N2=42, so 8 + 29×14 + 42×4 = 582 bytes. The structure is validated; some field-to-state and AI relationships remain open (document 08).

## Other formats (older copy unless noted)

| Extension | Count | Confirmed observation or limitation |
|---|---:|---|
| `.STS` | 28 | 1,344-byte tables loaded per fighter; 16-bit values and FFFF sentinels. |
| `.MVS` | 30 | 96 moves per bank: three sequences, three displacement streams, transitions and four control bytes per descriptor. `RBMG`/`RBMN` use big-endian pointers. Details in document 09. |
| `.A0C`–`.A5C` | 28×6 | 500-byte masks selecting frames to load in `FUN_1a23f`. |
| `.MRW` | 69 | 1,142 unsigned 8-bit mono PCM samples: u16 count, then u32 offset/size pairs. WAV exports in `EXTRACTED/audio/mrw/`. |
| `.MRS` | 68 | Sequences controlling MRW samples; 48–21,156 bytes. Exact triggering and pitch remain under analysis. |
| `.CL2` | 52 | `CLL5` or `CLL6` magic; validated collision boxes in 28 fighter banks and other background banks (document 08). |
| `.CTL` | 23 | Background scroll/animation scripts; see document 08. |
| `.DAT` | 6 | CHRSET1–3 (3,456 bytes each), OPTIONS, SOUND (524 bytes), OPTW95. `SOUND.DAT` configures sound cards/drivers. |
| `.ANI` | 3 in older copy; 105 in Director's Cut | LLOGO/END/ENL RLE/delta codec decoded, 161 frames exported. Older copy lacks long movies (`ENERGY.NFO`); Director's Cut supplies them, including `RQLINK.ANI`. See document 09. |
| `.TXT` / `.FRA` / other languages | — | Readable game text with formatting tags. |
| `.ISW`, `.BIN`, `.RAW` | One each | OPTIONS.ISW, BG.BIN, TESTD.RAW: still to analyze. |
| `.RST` | 1 | STATE.RST: ASCII INI. |
| `.LST` | 1 | EXTRA.LST: ASCII list. |
| `.GLL` | 1 | GRIP.GLL: structure unknown. |
| `.386` | 3 | HMIDET/HMIDRV/HMIMDRV audio drivers. |

## Binary entry points in the older EXR

| Address | Role |
|---|---|
| `3c731`, `3c9d0`, `3caac` | GGF LZW, palette and bit reader |
| `1a0fc`, `1a23f` | Complete or selective ANL/ANR loading |
| `1c819` | Pixel-span decoder and sprite drawing |
| `20dda` | Second fighter palette shift (+70 when index <70) |
| `1a473` | Two 70-color palettes |
| `35247` | AIP parser |
| `23e97` | MVS/STS loader |
| `1b304`, `1b39e`, `1b3e6` | DOS open, read and close |
| `121c3` | Whole-file read |

## Next work

- [x] Extract AIP/MRW/MVS/STS/CTL/CL2 structure into private JSON.
- [x] Convert MVS to JSON, MRW to PCM WAV and CL2 to JSON.
- [ ] Finish unknown AIP/STS fields and MRS sequences.
- [ ] Refine CTL scripts and contextual palette assignment for other ANR banks.
- [ ] Export remaining ANR banks, CHRSET fonts and the additional Director's Cut ANI files.

Historical guesses remain in the journal for context; use the validated structures above for implementation.
