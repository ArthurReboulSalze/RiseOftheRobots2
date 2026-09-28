# 12 — English UI flow and reference screens

The original screenshots supplied on 2026-09-26 are private visual references under `documentation/captures/reference/`. Their French text is translated for the port. This page distinguishes the current flow from presentation work still to implement.

## Title and options

The title uses the original RESURRECTION / RISE 2 artwork and a centered menu:

```text
START
OPTIONS
PLAYER
HIGH SCORE
CREDITS
QUIT
```

Up/down select; Enter confirms. OPTIONS contains MUSIC VOLUME, GAME VOLUME, EASY FINISHINGS, DISPLAY FILTER, KEY MAPPING and BACK. Left/right adjust values, Enter toggles or opens a row, and Escape returns. The original localized footer is covered before drawing the English controls. Settings persist per private profile; [document 16](16_options_filters_and_fx.md) specifies defaults, finishing shortcuts and filters.

## Key mapping

**OPTIONS → KEY MAPPING** opens two keyboard columns, PLAYER 1 and PLAYER 2. Each has ten entries:

```text
UP
DOWN
LEFT
RIGHT
PUNCH LIGHT
PUNCH MEDIUM
PUNCH HEAVY
KICK LIGHT
KICK MEDIUM
KICK HEAVY
```

Up/down choose an entry, left/right choose a player, and Enter waits for a replacement key. Escape cancels a pending assignment; otherwise it returns to OPTIONS. The labels replace the earlier opaque 01/02/03 and P1/P2/P3 names. Bindings use DOS set-1 scancodes in the profile's private `ui/rise2.cfg`, converted to SDL keys at runtime. Joystick configuration remains future work.

## Character selection

The current screen has a two-row grid of twenty visible portraits, red and blue player cursors, and larger selected portraits beneath it. Player 1 uses arrows; player 2 uses A/D. Enter starts the selected match. Continuing beyond the visible grid exposes the remaining robots, capped to the imported edition's 28 or 30 available banks.

Names, portrait palettes and banks follow source order ABC…Z01, with 2/3 in Director's Cut. A is Cyborg and 0 is Surpressor; the previous two-position name rotation is corrected. Native unlock progression has not been reconstructed. The full hangar background, animated selection robots and exact original confirmation behavior remain presentation work.

If original standalone name WAVs were imported as supplemental media, changing a selection speaks that robot's name. Victory announcements use the normal robot MRW sample 14, independently of supplemental WAVs. The standalone name location in base DOS data remains unconfirmed; see [document 17](17_finishings_voices_and_player.md).

## Movie player

**PLAYER** browses the profile's converted ANI and optional FLC movies. Up/down selects, Page Up/Down moves ten entries, left/right changes category, and Tab cycles high/low/both resolutions. Enter or Space plays. A robot ending queues END or ENL before returning to the browser. Source placeholder cards are explicitly labelled.

During playback, Space or Enter pauses, left/right seeks ten frames, Home/End jumps to the first/final playable frame, and Escape returns. Frames retain their aspect ratio and recovered timing; long movies stream one texture at a time. Menu music pauses. Full campaign progression, original movie audio and ending text remain to integrate.

## Versus reference

The supplied VS reference shows a dark patterned backdrop, large opposing robot silhouettes, a central VS, a circular loading indicator and names at the lower edges. The asset is available, but the complete transition and loading presentation remain to implement. Do not treat a decoded VS image as evidence of the complete original flow.

## Combat reference

The source screenshots show player names and scores at the top edges, two mirrored health bars, a red central timer, small player power pictograms, bottom super indicators, and a scrolling arena with textured ground. The port now draws the combat HUD at the reference's 640×400 coordinates using the original 12×12/24×24 CHRSET glyphs. Pictograms are specific to each robot: `FUN_132b6` reads an 18-word initial-power table at `RISE2.EXR:0x50660`, sets the corresponding bit in each fighter's power mask, and `FUN_16d4e` renders set bits with CHRSET glyphs `v` through `{`. `TOOLS/extract_power_icons.py` extracts that table from either verified EXR edition during import. Acquired power bits are also shown in a fight. The mapping is proven for the standard A–R slots; additional/hidden slots currently show no initial pictogram until their original rule is identified. The colour glyphs use the shared `EXTRA.PAL` source palette. The 90-second display counts active 25 Hz fighting ticks and freezes with the match; it currently does **not** end a round at zero. Scores display zero until original scoring and round progression are implemented. Health and super indicators use live fighter state; exact colour remapping and power-mask changes still need DOS playback comparison.

F1 pauses to show both robots' source commands and configured controls, including finishing shortcuts when enabled. F2 switches player 2 between basic CPU sparring and its configured keyboard. See [combat commands and FX](15_combat_commands_and_fx.md) for supported behavior and limits.

## Pause menu

Escape opens the pause menu over the live arena, in the original 24×24 font and near the source capture's lower-screen position:

```text
CONTINUE MATCH
# CALIBRATE JOYSTICKS #
$ QUIT MATCH $
```

The `#` and `$` characters are the source font's F9/F10 keycap glyphs (as used by the original `RISE.FRA` strings), rather than literal punctuation on screen. The overlay freezes combat without darkening the entire scene. Continuing resumes it; quitting returns to selection. Joystick calibration is currently a placeholder. The old F1/F2 help footer has been removed from normal play; the shortcuts remain available.

## Rendering and data

Screens compose at 640×400, displayed in a resizable window with preserved aspect ratio and the selected filter. Title/menu backgrounds are GGF resources; portraits are VSFACE/V4FACE; arenas are 800×400 AG-family images. `TOOLS/extract_chrset.py` decodes owned CHRSET1/2/3 files and the shared `EXTRA.PAL` during import into private `ui/charset1.png` through `charset3.png`; no glyph artwork is shipped in the repository. Other menus still use the earlier generated font until their original layouts are reconstructed. Original text rendering was identified at `FUN_1fa80`. Decoded assets and reference screenshots remain private.
