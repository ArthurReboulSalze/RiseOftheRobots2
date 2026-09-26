# 16 — Options, display filters and corrected combat FX

## Options and saved controls

The title menu contains START, **OPTIONS**, HIGH SCORE, CREDITS and QUIT. OPTIONS opens the imported OPTIONS background and contains:

| Setting | Controls and behavior |
|---|---|
| MUSIC VOLUME | Left/right change by 5%, from 0 to 100; controls SDL_mixer's music channel |
| GAME VOLUME | Left/right change by 5%, from 0 to 100; controls all sound-effect channels |
| EASY FINISHINGS | Left/right or Enter toggles the post-KO attack-button shortcut |
| DISPLAY FILTER | Left/right cycle six modes; Enter advances to the next |
| KEY MAPPING | Enter opens both players' direction and light/medium/heavy attack bindings |
| BACK | Enter returns to the title |

Up/down select a row. Escape returns from key mapping to OPTIONS, then to the title. Key mapping is no longer a title-menu entry. Existing DOS scan-code bindings remain in the private `EXTRACTED/ui/rise2.cfg`; F-key names now agree with their actual set-1 codes. Other options are saved immediately to `EXTRACTED/ui/port_options.json`, independently for each imported profile. Missing or corrupt JSON uses defaults: both volumes 100%, easy finishings off, original pixels. Volume zero mutes its respective channel group; the original per-sample gain and 44.1 kHz conversion remain in place.

## Easy finishings

The assist is a port option, not a recovered DOS rule. It operates only for the winner during the existing finishing window. Available variants come from that robot's command directory and nonempty even movements 48–58. They are deduplicated and ordered by movement ID. The supplied banks generally expose one finishing command; the assist does not assume that every robot has two or three.

| Available variants | Configured shortcut |
|---|---|
| One | Any of the six punch/kick buttons |
| Two or three | Light punch or kick: first; medium punch or kick: second; heavy punch or kick: third, if available |
| More than three | Punch light/medium/heavy, then kick light/medium/heavy, in order |

A held knockout attack does not trigger the assist: release and press again. The assist grounds the fighters, faces them toward one another and places the winner at a demonstration distance before starting the source action. Original animations, MVS effects and signed CL2 victim reactions then run through the regular combat pipeline. The defeated player's buffered input is discarded. F1 shows available shortcuts; disabling the option restores command-only finishings.

Missing actions remain unavailable. Some native finishing callbacks, linked cinematics and DOS progression restrictions are still unimplemented; a shortcut does not make those presentations complete.

## Display filters

All filters process the composed 640×400 scene, preserving its aspect ratio when the window is resized. They affect menus, fighters, FX and HUD together and do not change the 15 Hz simulation clock.

| Mode | Rendering |
|---|---|
| ORIGINAL PIXELS | Nearest sampling and integer display scaling; default |
| BILINEAR | SDL linear sampling; inexpensive soft scaling |
| SCALE2X | Edge-preserving 2× reconstruction with no new palette colors |
| SCALE3X | Edge-preserving 3× reconstruction with no new palette colors |
| XBR SMOOTH | CPU 3× xBR-lv2-style corner reconstruction and color interpolation |
| CRT SOFT | 2× soft interpolation, mild scanlines and an RGB mask |

Scale2x/Scale3x follow the [published Scale2x algorithms](https://www.scale2x.it/algorithm). The xBR adaptation follows [Hyllian's xBR-lv2 shader](https://github.com/libretro/glsl-shaders/blob/master/xbr/shaders/xbr-lv2.glsl), using its smooth-corner variant; `filters.cpp` retains the complete MIT copyright and permission notice. These are established filters, not newly released algorithms. CRT is a lightweight approximation, not a full CRT shader simulation.

CPU filters run only when the scene or selected mode changes. xBR shares luminance/border data and precomputed corner weights, with up to four row workers. The last filtered texture is reused while the scene is unchanged. Runtime performance depends on the renderer and machine; use bilinear or Scale2x if the more expensive modes feel slow.

## Cyborg FX palette and roster identity

The source roster follows A–Z, then 0/1, with 2/3 in Director's Cut. RISE.FRA's first robot-name section and VSFACE's palette/portrait order agree. **RBTA is Cyborg**, RBTC is Prime 8, RBTF is Rook and RBT0 is Surpressor. The previous port displayed names two positions away from their banks; `roster.h` now uses source order directly for names, bank IDs and portraits.

The reported Cyborg move is **down, diagonal forward, forward, punch**, MVS movement 81. Its attached script uses robot images 361–366, corresponding to atlas frames 362–367. It was already positioned correctly. Its painted pixels use shared palette entries including 206, 212, 213 and 227; the arena palette reserves those entries as green. That export error flattened the white/grey/green source artwork into green.

The robot exporter now replaces indices 203–239 with the corresponding **RGB8 entries of EXTRA.PAL**, after its eight-byte header. It preserves the robot's own colors, the opponent range, and every painted span. It does not remove green pixels: the original green ramp is part of the effect. RGB outside spans is cleared with alpha, preventing hidden reserve colors from contaminating filtered edges. The same correction applies to RBT and RB4 exports, while standalone EXTRA continues using its own full palette.

Slot 253 and native dynamic palette changes remain separate research work; they are not guessed or made transparent. Opponent-color indices in static previews still repeat the owner's palette, as before. These limits do not affect the verified Cyborg uppercut correction.

## Shared projectile coordinates

In the older EXR, `FUN_191be` adds the owner's position to attached-script offsets; `FUN_192da` uses the stored projectile position and direction. Both feed the same blitter. The bank flag changes the sprite lookup (robot `image+1`, EXTRA `image+1000` in the native resource table), not the coordinate convention.

The port previously assigned shared EXTRA visuals an independent 320/200 pivot and scale. They now use the owner's authored canvas projection and sprite scale, including camera displacement and captured projectile direction. Projectile collision boxes use the same projection as their pixels. Ordinary contact impacts still anchor to their overlap point. Cyborg's already-correct robot-bank attached offset is preserved.

## Refresh and verification

Both local runtime profiles have been refreshed. A new full import applies the correction automatically. Other existing profiles can regenerate their atlases without importing CDs or audio again:

```powershell
python TOOLS/extract_anr.py --source LOCAL/my-game/game --output LOCAL/my-game/EXTRACTED/sprites
```

The Release build and four CTests cover saved options, invalid settings, filter edges/colors, normal combat and finishing shortcuts, plus existing fighter/audio checks. Python fixtures check shared palette replacement, painted index-zero opacity and transparent RGB clearing without game files. Private checks use the actual 28/30-robot imports:

```powershell
PORT/build/Release/rotr2_asset_smoke.exe LOCAL/my-game/EXTRACTED LOCAL/my-game/checks/options
PORT/build/Release/rotr2_headless.exe LOCAL/my-game/EXTRACTED LOCAL/my-game/checks/options
```

The menu check verifies volume/mute adjustments, persisted settings, key rebinding and return navigation, and captures all six filters. Headless combat reproduces Cyborg's quarter-circle punch in both directions with visible white FX details, a real EXTRA projectile hit, and Prime 8's ordinary/easy finishing with a signed CL2 death reaction. Captures and game data remain private. Automated results do not replace interactive visual and gameplay comparison with DOS.
