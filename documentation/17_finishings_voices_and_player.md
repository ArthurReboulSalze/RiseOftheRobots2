# 17 — Finishing distances, original announcements and the movie player

Status: September 27, 2026. Addresses below refer to the older DOS `RISE2_EXR` analysis. Original recordings, decoded frames and decompiler output remain private. A normal import needs no bonus media or Ghidra installation.

## Cyborg's finishing distance

The single-button assist previously put every winner 100 logical pixels from the opponent. This started Cyborg's action but left its decisive attack box out of range. It was a placement failure, not a missing movie.

Cyborg is slot A (`RBTA`). Its finishing command selects movement 58: forward, down-forward, down, down-back, back, punch. The source's newest-first command bytes are `[1,4,20,16,18,2]`. The animation reaches image 333 after images 33–39. Image 39 has an ordinary attack; image 333 carries the decisive signed CL2 value **197 = -59**, selecting victim movement **59**. The finishing reaction is therefore determined by collision data, not by guessing `attacker_move + 1`.

For image 333, the raw decisive box is `(115,103,30,15)`. With the native x projection `4*x - 294`, its horizontal interval is 166–286 pixels relative to the attacker. The old 100-pixel assist missed it. Against Surpressor's stagger body, the corrected assist chooses **226 logical pixels** and reaches victim state 59.

The assist now scans the selected source finishing sequence, includes its horizontal displacement, and aligns a signed death attack box with the opponent's current body box. It grounds and faces both fighters, clears residual pushback, and shifts the pair together near a wall to preserve the calculated gap. This remains a port accessibility option. It does not impose automatic placement when entering a normal finishing command. Release the KO attack and make a fresh attack press during the finishing window.

### Native range gate: confirmed code, unresolved indexing

`FUN_2695b` checks finishing/debug and boss/unlock conditions for even actions 48–58. It also compares absolute fighter-anchor separation with signed bounds read at `0x6279a` and `0x6279c`, subtracting 320. The executable contains 28 or 30 bound pairs in robot-like order. Examples after subtracting 320 are:

| Apparent slot | Older build | Director's Cut |
|---|---|---|
| A / Cyborg | -8–260 | 72–260 |
| B / Loader | 60–316 | 60–316 |
| C / Prime 8 | 28–172 | 28–172 |

However, the inspected assembly indexes the pair with the **player-side argument, 0 or 1**, rather than a proven robot index. No relevant table writer has yet been identified. The exporter preserves these raw pairs as `finishing_distance_hints`; the port uses an apparent robot pair only as an assist fallback when no decisive collision is available. This is an explicit interpretation, not evidence of an exact per-character native eligibility rule. Manual range gating remains unresolved.

## Victory speech from normal game banks

The original robot MRW banks already contain victory announcements. The user listened to `RA_14.wav` and confirmed **“Cyborg win”**. `RA_13.wav` was separately identified as a sound effect, not the standalone robot name.

This agrees with the post-round code in `FUN_137ce` / `FUN_15403`: the branches near `0x155f1` and `0x1560c` request sequence **14** through `FUN_3c28c`, choosing the corresponding player's loaded bank. Cyborg's MRS sequence 14 uses selector 0 and sample 14 at the normal `0x10000` rate multiplier. The port therefore reads `audio/mrw/R<slot>/R<slot>_14.wav` and announces the winner once when the round enters its ending phase. Only Cyborg's spoken words have been manually confirmed in this session; the other slots use the same source convention.

These base recordings are unsigned eight-bit mono PCM at 11,025 Hz. They use the existing filtered conversion to the port's 44.1 kHz output. No extra disc or separately supplied WAV is required for this victory path.

### Optional standalone names

The inspected Director's Cut bonus filesystem has **77 WAV files**, including standalone names for all 30 robots, taunts and generic announcements. The user confirmed that `CYBORG.WAV` says **“Cyborg”** alone. These standalone name recordings are 16-bit mono PCM at 44,100 Hz.

`extract_bonus_media.py` preserves the original WAVs and builds `audio/voices/manifest.json`. The runtime speaks a supplied standalone name when a selection changes. It reserves mixer channel 0 for speech so combat effects do not replace it. Game volume controls both effects and announcements. Where supplied, a full matching victory WAV can replace the base recording; otherwise the native MRW sample 14 remains available. Optional generic FIGHT and YOU WIN recordings are used only when present and applicable. No speech is synthesized or cut from another phrase.

**The standalone selection-name location in normal DOS game data is still unconfirmed.** Neither the optional bonus recordings nor the unsuccessful automated transcription attempts prove it absent. Sample 13 must not be assigned this role. Missing supplemental media leaves the normal victory announcements and game import working.

## All supplied ANI movies

Director's Cut Disc 1 provides **105 ANI files**. The older stripped installation provides only `LLOGO`, `END` and `ENL`; its `ENERGY.NFO` explicitly records removed cinematics. All 105 supplied ANI layouts are now converted and catalogued:

| Family | Canvas / content | Role |
|---|---|---|
| LLOGO | 320×200 / 199 rows | Logos and legal introduction |
| END / ENL | 640×400 / 200 rows; 320×200 / 100 rows | Shared epilogue |
| R?VICT / R?VICL | Same high/low layouts | Robot endings |
| R?LINK / R?LINL | Same high/low layouts | Linked animations; exact gameplay callbacks remain incomplete |

The initial decoder now follows `FUN_32171` / `FUN_323d4`: it skips each row's leading command-count byte and decodes signed RLE until the row width is filled. `RZVICT` has a stale count byte; trusting it misaligned the palette. Delta frames use signed row skips and two-pixel words. The palette is VGA six-bit RGB, with index 0 forced black.

Both DOS players stop at **declared frame count minus two**. Six shipped files have a truncated final record (`RGLINK/LINL`, `RKLINK/LINL`, `RXVICT/VICL`), beyond the playable range. The converter validates every active frame and does not read those unused records. LLOGO retains all 39 decoded frames for inspection but plays 37; END/ENL play and export 59 each. This supersedes the earlier 161-frame export description.

Some LINK files are original **“No Animation Yet…”** cards. Exact copies of the known R2LINK/R2LINL cards are marked SOURCE PLACEHOLDER in the catalogue; the port does not invent replacement movies. LINK must not be assumed to be a finishing cinematic. `FUN_17e7d` does establish a robot's `R?VICT` followed by END and ending text at campaign completion.

### Recovered movie clock

The hardware interrupt is **100 Hz**, but the movie counter is **25 Hz**. Initialization at `0x11101/0x1110a` sets `DAT_663a2 = 25` and `DAT_663a0 = 100`. The PIT setup at `FUN_1b658/1b745` uses divisor 11931. Handler `FUN_1b77c` calls `FUN_1444d`, whose accumulator advances `DAT_6636a` once per four interrupts. Treating the IRQ frequency as the playback clock made the movies four times too fast.

Exported manifests use the normal 25 Hz counter and native wait parameters:

| Movie | Initial slow frames | Waits |
|---|---:|---|
| LLOGO | 11 | Initial frames: 20 ticks; ordinary frames: 2 ticks; final hold: 25 ticks |
| END / ENL | 1 | Same waits |
| LINK / LINL / VICT / VICL | 6 | Same waits |

The parameters come from the LLOGO call near `0x35221–0x35232` and wrappers `FUN_346f8`, `FUN_34742`, `FUN_3478c`. Ordinary ANI motion is consequently **12.5 frames/second**, with longer initial holds. Alternate DOS slowdown states exist and remain outside this default playback model. Movie timing is independent of the port's combat scheduler, now also running on the recovered 25 Hz logical cadence; [document 08](08_engine.md) records the combat correction.

## Optional FLC movies

The inspected bonus filesystem contains **70 FLC files**, including eight- and sixteen-bit images. A locally installed FFmpeg converts these to PNG frames; all 70 supplied files converted with their complete declared frame counts. FLC header speed is in milliseconds, unlike ANI counter ticks. `ATTR.FLC`, for example, has **3,802 frames at 42 ms/frame**, approximately **160 seconds**. The short ANI logo sequence is not this longer film.

Bonus titles retain source filenames. Their story and gameplay roles are not guessed. Combined with the 105 ANI entries, the local complete-media catalogue contains **175 movies**, including both resolution variants and original placeholder cards; this is not 175 distinct story scenes.

## PLAYER menu

The title menu now contains START, OPTIONS, **PLAYER**, HIGH SCORE, CREDITS and QUIT. PLAYER browses only movies present in the selected private profile.

| Control | Browser | Playback |
|---|---|---|
| Up / down | Select a movie | — |
| Left / right | Change category | Seek by ten frames |
| Tab | High resolution / low resolution / both | — |
| Page Up / Page Down | Move ten entries | — |
| Enter | Play; a robot ending queues the matching epilogue | Pause / resume |
| Space | Play | Pause / resume |
| Home / End | — | First / final playable frame |
| Escape | Return to title | Return to browser |

Categories are all videos, robot endings, linked animations, intro/epilogue and bonus videos. Frames preserve their aspect ratio inside 640×400, including 640×480 FLCs. Playback streams one decoded texture at a time instead of loading a long film into video memory. The small startup LLOGO remains preloaded. Menu music pauses during PLAYER playback.

## Import and refresh

A full import converts all recognized ANI files automatically. Original optional WAV/FLC media can be supplied as another `--source` folder, archive or disc image. They are copied separately into `media/`, never merged into a different game edition. FFmpeg is needed only for optional FLC conversion and is never downloaded automatically. If unavailable, the originals are retained and the report records the limitation.

```powershell
# Refresh movies and combat metadata in an existing private profile
python TOOLS/extract_ani.py --source-dir LOCAL/my-game/game --output-dir LOCAL/my-game/EXTRACTED/video
python TOOLS/extract_combat.py --source LOCAL/my-game/game --output LOCAL/my-game/EXTRACTED/data/combat.json

# Optional original WAVS/FLICS folders supplied by the owner
python TOOLS/extract_bonus_media.py --source "D:/Original Media" --media-output LOCAL/my-game/media --extracted LOCAL/my-game/EXTRACTED
```

## Validation and remaining work

The Release build, four CTests, 33 Python tests and nine image-codec fixtures pass. Synthetic checks cover stale ANI row counts, active-frame truncation, normal imports without bonus media, and base victory speech without a voice manifest. Real private profiles with 28 and 30 robots pass menu, pause/seek, ending-to-epilogue and first/middle/last-frame checks for all 175 movies. Streamed movies retain one texture. Cyborg's finishing reaches victim movement 59 in eight combinations of player side, facing and wall placement. Captures of PLAYER and the finishing were inspected; this does not replace interactive gameplay comparison.

The full campaign, native finishing callbacks and exact manual finishing range gate remain incomplete. Standalone names in base DOS banks remain to locate. MRS music sequencing, original movie music/cues, palette fades and ending text are not yet reproduced by PLAYER; recovered video frames must not be described as complete audiovisual campaign playback. The desktop profile still has the older 28-robot game banks, supplemented locally with the recovered Director's Cut media; the separate Director's Cut profile has all 30 robots.
