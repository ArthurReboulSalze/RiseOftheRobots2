# 09 — Remaining resource formats

This page separates confirmed formats from work still needed. Addresses refer to the original DOS `RISE2_EXR` image unless otherwise stated.

## MVS: movement data, not sound

Each robot's MVS bank contains 96 movement descriptors. The normal header begins `MVS\x01`, followed by two dwords and a directory of 96 u32 offsets starting at `0x0c`. Descriptors are 32 bytes long. `FUN_23e97` relocates self-relative pointers using `base + offset - 0x0c`. The extra Director's Cut banks `RBMG` and `RBMN` use big-endian values and reversed magic.

| Descriptor offset | Meaning |
|---|---|
| `+0x00, +0x04, +0x08` | Pointers to animation sequence and movement data |
| `+0x0c, +0x10, +0x14` | Three signed horizontal-displacement sequences; little-endian for RBT, big-endian for RBMG/RBMN |
| `+0x18` | Transition pairs: u16 input mask, u16 target movement; terminated when the first word is negative |
| `+0x1c` | Flags |
| `+0x1d` | Automatic target movement |
| `+0x1e` | Resume index |
| `+0x1f` | Repeat or vertical-impulse field |

An animation sequence consists of two-byte entries; `image = 2*b0 + (b1 & 1)` and `b0 == 0xff` ends it. MVS image `i` maps to atlas frame `i+1` because RBT atlases begin with an empty entry, and to CL2 record `i`. In RBT0, descriptor `0x190` is idle: sequence at `0xd90` yields images `0,0,1,1,2,2,3,3,2,2,1,1` and then `ff`; transitions at `0xdaa` include masks 0x10/0x14/0x12 to movement 7 and 0x04 to movement 2. The idle displacement is zero. `FUN_217a4` reads transitions at `+0x18`; `FUN_234fa` chooses the next move; `FUN_21baf` applies horizontal deltas with a factor of two and facing direction. The 30 converted banks contain 2,880 movements. Exact physical-button mapping and DOS timing remain to be verified.

## MRW: digital audio samples

MRW files contain unsigned 8-bit mono PCM. The silent midpoint is `0x80`. Each file starts with a u16 count and then `count` pairs of absolute u32 offset and u32 size. The original 69 banks yield 1,142 samples and account for approximately 99.8% of file bytes. For example, R0 entry 0 has offset `0x9a` and size 2,769. Director's Cut Disc 1 contains 71 banks and 1,179 entries, including the extra robots.

The base export rate is 11,025 Hz. SOS playback can change it: from `FUN_3a792` through `FUN_417c9` at `0x3a893`, the rate is `(pitch_16_16 * 11025) >> 16`. The exact per-event pitch source is still being traced. The extracted samples were checked by listening; they are intelligible effects, unlike attempts to interpret MVS as PCM.

## MRS: sample sequences

The 68 MRS sidecars range from 48 to 21,156 bytes. `FUN_3be83` loads MRS sequences and their MRW bank; `FUN_3a6b0` places sequence data into `DAT_705bc`. The event trigger and pitch mapping are not yet fully decoded. Preserve MRS when importing.

## SOUND.DAT: audio driver configuration

`SOUND.DAT` is a 524-byte driver-configuration file, not a sound-event table. The read at `0x272d5–0x2732e` fills `DAT_70428`; `FUN_2cbc1` processes 12-byte records; `FUN_3a3e8` initializes the HMI SOS driver (`HMIDRV.386`). Its `0xe0xx` values identify drivers.

## ANI: cinematics and menus

The original DOS copy contains only three ANI streams: `LLOGO` (39 frames at 320×200), `END` (61 at 640×400), and `ENL` (61 at 320×200). Each frame uses row-oriented RLE, a 768-byte six-bit palette, and later delta rows. All three decode with exact stream consumption. Their 161 images do not imply 161/15 seconds of video: the DOS player controls frame waits separately.

`ENERGY.NFO` in the original installation explicitly says cinematics were removed. `RQLINK.ANI` is absent there. Director's Cut Disc 1 contains 105 ANI files; the additional 102 are retained by the importer but await decoding and sequence/timing analysis. Disc 2 supplies bonus material and is not needed for game content.

## Fonts

`CHRSET1.DAT` through `CHRSET3.DAT` are 3,456 bytes each. `FUN_1fa80` renders text using a table at `0x6250f`; the glyph encoding and layout remain to be decoded.

## Music sources

The original game supports CD audio and digital music. The user's rip has nine CDDA tracks numbered 02–10; track 01 is data. The MSCDEX wrappers at `0x433e6/0x43410/0x43445/0x43564` are called through HMI SOS code at `0x40718/0x40b7b/0x40d58`. Track 02 as title music and 03–10 as combat music are hypotheses, not yet confirmed in play.

`README.FRA` and `OPTIONS.TXT` document `DIGITAL MUSIC` and `CD STREAMED MUSIC`. `FUN_150b4` uses `DAT_6639c` to select 0 = ambience, 1 = digital music, 2 = CD audio. The initial digital name is `BGA.MRW` at `0x50863`; digital mode changes the prefix to M and chooses A + arena modulo 6, giving `MGA` through `MGF`. `FUN_3c17c` calls `FUN_3be83` to load MRS/MRW. There is no evidence of MID/HMI/HMP/XMI music files. The importer records the available mode; music playback in the port is still pending.

## Next reverse-engineering tasks

Decode the 102 additional ANI files and their timing; finish MRS triggers and pitch; map the ten physical controls to the MVS masks; decode CHRSET fonts; and verify CDDA selection and digital-music playback against the DOS game.
