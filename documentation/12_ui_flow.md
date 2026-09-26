# 12 — English UI flow and reference screens

The original screenshots supplied on 2026-09-26 are private visual references under `documentation/captures/reference/`. Their French text is translated for the port. This page distinguishes the current flow from presentation work still to implement.

## Title and options

The title uses the original RESURRECTION / RISE 2 artwork and a centered menu:

```text
START
OPTIONS
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

## Versus reference

The supplied VS reference shows a dark patterned backdrop, large opposing robot silhouettes, a central VS, a circular loading indicator and names at the lower edges. The asset is available, but the complete transition and loading presentation remain to implement. Do not treat a decoded VS image as evidence of the complete original flow.

## Combat reference

The source screenshots show player names and scores at the top edges, two mirrored health bars, a red central timer, yellow super indicators, and a scrolling arena with textured ground. The current port has an initial HUD, health/super state, source animations, hits, FX and a finishing/result/rematch phase. Full score, timer and round progression still need integration.

F1 pauses to show both robots' source commands and configured controls, including finishing shortcuts when enabled. F2 switches player 2 between basic CPU sparring and its configured keyboard. See [combat commands and FX](15_combat_commands_and_fx.md) for supported behavior and limits.

## Pause menu

Escape opens the existing pause menu:

```text
CONTINUE MATCH
F9  CALIBRATE JOYSTICKS
F10 QUIT MATCH
```

The overlay freezes combat. Continuing resumes it; quitting returns to selection. Joystick calibration is currently a placeholder.

## Rendering and data

Screens compose at 640×400, displayed in a resizable window with preserved aspect ratio and the selected filter. Title/menu backgrounds are GGF resources; portraits are VSFACE/V4FACE; arenas are 800×400 AG-family images. The port's font renderer uses imported font data; original text rendering was identified at `FUN_1fa80`. Decoded assets and reference screenshots remain private and are not shipped in the repository.
