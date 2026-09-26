# 08 — DOS combat engine map

Analysis starts at the blitter (`FUN_1c819`) and follows the combat cluster in `RISE2_EXR`. Addresses are Ghidra linear addresses for the original DOS executable loaded at 0x10000, not for the Director's Cut EXR. The full graph is in `ANALYSIS/exr_callgraph.txt` and the function index in `ANALYSIS/exr_functions.txt`.

## Main loop and functions

| Function | Role |
|---|---|
| `FUN_137ce` | Round controller: initialization and `FUN_13d8e` intro, `FUN_2163a` combat update, `FUN_165fb` render, KO check, arcade wait, and perfect-win bonus of 1,000 points when the opponent retains the full 120 HP |
| `FUN_2163a` | Combat-frame update for both players: inputs, `FUN_234fa` state machine, `FUN_21baf` physics, camera, and HUD |
| `FUN_25615` | Facing update for the two players; earlier notes incorrectly treated it as the complete frame step |
| `FUN_234fa` | Input-to-move state machine, chaining, transitions, and stun |
| `FUN_197b6` | Keyboard attack filtering and facing-relative direction masks |
| `FUN_2298b` | Input smoothing, at most four units per frame |
| `FUN_21baf` | Physics and collision update; called six times at round start and twice per frame |
| `FUN_231ac` | Fixed-point vertical velocity, gravity and landing |
| `FUN_35a7c` / `FUN_35eb8` | CPU controller and decision-making |
| `FUN_12cc6` / `FUN_14c04` | Match and robot loading |
| `FUN_15403` | Screen controller and availability mask near `0x62998` |
| `FUN_1fa80` | Character-by-character text renderer, table at `0x6250f` |
| `FUN_20fb9` | Player initialization |
| `FUN_165fb` / `FUN_1aa37` | Combat render and blitter wrapper |
| `FUN_381a9` / `FUN_3885e` | Mutual box collision tests |
| `FUN_253a8` | Pushback and wall handling |
| `FUN_244eb` | Projectile slots |
| `FUN_22848` | Per-player particle effects |
| `FUN_3a0d2` | Background scrolling channels |
| `FUN_38b72` | Hit application and damage |
| `FUN_1b22a` | KO sound |
| `FUN_13d8e` | Round intro |

The camera moves when the fighters' horizontal separation reaches approximately 100 units. `DAT_65fd6/65fda` holds arena bounds. The original health maximum is 120 HP.

## Player state

Player 0's movement ID is at `0x6620e` and player 1's at `0x662a5`; stride is `0x97` (151 bytes). The dword at `0x6620c` has the movement ID in its high word; the sprite-image index is a separate word at `0x66210`.

| Offset | Field and evidence |
|---|---|
| `+0x00` | u16 state ID |
| `+0x02` | u16 current sprite-image index, used by CL2 |
| `+0x04` / `+0x06` | Arena x / y positions |
| `+0x08` | Frame within state |
| `+0x0e` | Signed 8.8 vertical velocity |
| `+0x14` | Fractional vertical-displacement accumulator |
| `+0x16` | Flags: bit 0 facing, bit 1 CPU control, other bits state-dependent |
| `+0x2a` | Direction/button input mask |
| `+0x2c` | Health |
| `+0x2e` | Smoothed input value |
| `+0x46` | Ground y |
| `+0x44` | Fixed-point gravity |
| `+0x4e` | STS state-table pointer; record byte +6 bit 0x20 inhibits scripted interruption |
| `+0x6c` | Transition lock and adjacent freeze counter |
| `+0x6e` | Stun counter |
| `+0x79` | 0/1 marker checked by input flow; exact meaning unresolved |
| `+0x83..0x97` | AI variables |

Important runtime tables: `DAT_685cc/685d4` are relocated MVS movement pointers; `DAT_68524 + player*4 + move_id*0x14` addresses additional AIP movement records; `DAT_685dc + player*0x10` holds encoded combo input scripts with -1/-2 escapes; `DAT_70314 + player*0x3c` holds three 0x14-byte projectile slots. `DAT_65fe0` stores keyboard bits, six per player. Attacks use bits `0x01/0x20`, forward/back use `0x02/0x04`, up/down use `0x08/0x10`. At separation below `0x3d` (61 pixels), close-range moves add `0x40` to their move ID. [Document 14](14_combat_inputs_and_jumps.md) corrects the earlier input, STS and jump interpretations.

## Known state IDs

| ID | Working interpretation |
|---|---|
| `0x00` | Standing idle |
| `0x02/0x03` | Backward/forward walk |
| `0x07` | Crouch entry |
| `0x08/0x09` | Standing punch/kick |
| `0x0d/0x0e` | Stun/down reactions |
| `0x10` | Crouch hold |
| `0x12` | Single-frame automatic-return state; precise use pending |
| `0x14` | Hit reaction |
| `0x16` | Single-frame automatic-return state; precise use pending |
| `0x17` | Rise from crouch |
| `0x18/0x19` | Crouching attacks |
| `0x1a/0x21` | Automatic-return states; precise use pending |
| `0x20` | Neutral jump |
| `0x22/0x23` | Backward/forward jump |
| `0x25..0x2a` | Airborne attacks |
| `0x34/0x38` | Special reactions |
| `0x3c/0x3d/0x3e` | Movement states |
| `0x40/0x41/0x42` | Special attacks that inhibit input smoothing |
| `0x44/0x45` | Turning |
| `0x4a/0x4c/0x4d/0x4f` | Multi-stage attacks |
| `0x4b` | Timed special state |
| `0x54` | Possible finishing strike |
| `0x58` | Super attack at full meter (0x18 = 24), without normal collision |

Scripted interruption uses `(state & 0xf) < 10` and additional state checks, subject to STS byte +6 bit `0x20`. States above `0x4f` receive special defense handling. Ordinary transition tables are also evaluated separately.

`FUN_2163a` calls `FUN_25615` once before inputs and motion. It faces player 0 right and player 1 left when `x0 < x1`, reversing them when `x0 > x1`. Facing changes apply only to a listed subset of states: 0, 2, 3, 6, 0x10, 0x12, 0x16, 0x1a, 0x20–0x23, 0x3c–0x3e, 0x4a–0x4d, and 0x4f, or when the original pushback field permits an override. Ordinary attacks keep their orientation. In normal combat (link mode -1), turning exchanges diagonal jumps `0x22↔0x23`; `FUN_26321` preserves the current frame and only clamps it to the target's last visible frame if necessary. It does not restart the jump. States `0x20` and `0x4b` change facing without replacing the state. Crouch `0x10` enters turn `0x45`; other eligible states enter `0x44`. Additional pairs `0x4c↔0x4d` and `0x1a↔0x21` belong to the alternate linked mode. At equal X, player 0 flips only when both fighters are at ground Y and share a facing. [Document 14](14_combat_inputs_and_jumps.md) records the verified crossing rules and current port limits.

## Physics and movement: `FUN_21baf`

- `DAT_66285 + p*0x97` counts ticks between particle effects; the threshold at `DAT_62742` is indexed by the player's energy divided by 16. `FUN_3776e` supplies random values and `FUN_226cc` creates the particle effect; neither advances the fighter animation.
- `DAT_6627a` is post-hit freeze, decremented by 0x20; `DAT_6626c` is a countdown and `DAT_66270` a lock.
- `DAT_685cc + p*4 + (anim >> 16)*4` selects a movement table. Speed level from the player's low defense bits and `DAT_66256` selects one of three signed displacement streams. X changes by twice the displacement, signed according to facing.
- Energy falls by the per-frame cost in `DAT_66299`; at zero it clamps and sets `DAT_66398`.
- STS record bytes +5/+6 update runtime flags. STS byte +6 bit `0x80` flips facing at sequence completion; it is not an airborne marker.
- MVS bit `0x40` initializes a signed impulse from descriptor +0x1f and unsigned gravity from STS byte +4. `FUN_231ac` advances Y, clamps to the ground and selects the landing target. See document 14 for the exact arithmetic and verified trajectory.

## Hits and damage: `FUN_38b72`

1. A received hit adds 2 to super meter `struct+0x70`, or twice that when the attacker is not stunned; the cap is `0x18`.
2. Non-KO hits add 500 to the victim-indexed score field. Repeated matching states within `0x25` (37) frames increment a combo count.
3. Base damage is signed attack-box byte 4. `_DAT_70594 = base * multiplier(_DAT_705aa, default 1)`; final damage is `(_DAT_70594 * defense) >> 13`. Defense comes from the victim's state/configuration, including `0x50` for states above `0x4f` and `0x32` for one configuration bit. An attacking fighter stunned in states 4/0x14 deals one eighth damage; the minimum is 1.
4. Health at `struct+0x2c` falls by that amount in the relevant game mode. At zero, `FUN_3c28c` handles KO; otherwise `FUN_3c595` handles reaction.
5. The victim's freeze count rises by `_DAT_7059c`. Pushback and repeat-hit rules use `DAT_6624e` and `PTR_62b00[anim]`; airborne pushback uses 5.
6. The victim receives flag `0x04`. `FUN_39fde`, `FUN_22fc9`, `FUN_3c595` and `FUN_3c28c` choose sounds and reaction states.

Some names above remain working interpretations; validate against DOS play before using them as hard game-design rules.

## AI: `FUN_35a7c` and `FUN_35eb8`

`DAT_70518 + p*0x28` is the per-player action queue; `DAT_704ba + p*0x28` holds current AI input, which is written to the simulated player's input field. `DAT_66243` is an action timer and `DAT_66240` a one-shot input. `FUN_22957` returns player configuration: byte 0 is controller type, byte 1 difficulty (0–3).

The decision code charges an AI gauge at `DAT_66268 + p*0x97` by four per iteration during states 4/0x14 up to 140, and drains it otherwise. It reacts to an attacking opponent within `0xa1` (161) units using `FUN_3781a() % 100` against the per-robot threshold at `DAT_62ae5 + character*2`; successful reactions use `FUN_376c4`. Approach/retreat at `DAT_62af9` modifies AIP durations to 125% or 111% when closing and 80% or 90% when retreating. `FUN_36d20` and `FUN_36bb8` make final action choices; `FUN_3785c` executes the queue; `FUN_37db3` selects actions according to difficulty, including a modulo-90 roll above difficulty 1. Exact AI behavior remains to be ported.

## Projectiles, particles, background

`FUN_244eb` updates three projectile slots per player at `0x70314 + p*0x3c + slot*0x14`. Value -10000 marks free, -999 removal. A six-byte-per-frame script supplies velocity; one flag chooses an alternative collision table at `+0x87a0`. Bounds are ±0x17c (380 pixels).

`FUN_22848` updates five 12-byte particle slots per player at `0x6852c + p*0x3c`, using tables at `0x62702/0x62722`. `FUN_3a0d2` advances four 14-byte arena scrolling channels starting at `0x65c42`; CTL supplies their four subtables. `FUN_253a8` transfers wall pushback to the opponent at the arena bounds.

## CL2 collision records

Robot `R?.CL2` files use `CLL5` or `CLL6` magic; some background CL2 files are almost empty. `FUN_15d92` reads a 16-byte header and then records. Header bytes 8, 10, 12, and 14 contain u16 fields, with record count at +12. Each record begins with three counts `b0,b1,b2`, followed by `b0` five-byte attack boxes, `b1` six-byte body boxes, and `b2` five-byte single boxes. Its size is `3 + 5*b0 + 6*b1 + 5*b2`. The runtime builds frame pointers in a buffer at `0x65bd8`; the second player uses a separate area. The maximum is 500 entries.

| Group | Structure | Interpretation |
|---|---|---|
| `b0` | `[x,y,w,h,damage]`, five bytes | Attack boxes |
| `b1` | `[x,y,w,h,p4,part]`, six bytes | Defense/body boxes; part 5 is generic body |
| `b2` | `[x,y,w,h,tag]`, five bytes | Usually one per frame; likely push/footprint, exact use pending |

The game scales box x by four and y by two. `FUN_381a9` tests both fighter orientations via `FUN_3885e`. State `0x58` bypasses normal collision. `DAT_705b6` selects body/body, projectile/player, projectile/projectile, or arena/player modes. The deepest horizontal overlap wins. Projectile slots use alternative pointer tables, and arena collisions use background CL2 records.

Conversion of the original 28 robot files produced 9,204 CL2 frames, 1,119 attack boxes, 11,170 body boxes, and 7,091 single boxes. Of 51 total CL2 files, 49 consume exactly; `BGANIM1` and `BGANIMZ` have a secondary 75-byte section after 25 records, while `BMANIMB` has variant `6LLC` magic and is retained. The robot filename suffix uses the same base-36 alphabet as the assets.

## Background audio selection

`FUN_150b4` uses `DAT_6639c`: 0 = ambience, 1 = digital music, 2 = CD. The initial name `BGA.MRW` appears at `0x50863`. Digital music changes the prefix to M and selects A + arena modulo 6, yielding `MGA` to `MGF`. `FUN_3c17c` calls `FUN_3be83` to load an MRS sequence and its MRW bank. The original manual and `OPTIONS.TXT` confirm both digital and CD modes. See document 09 and the import guide for the unresolved playback work.

## Reproducing the analysis

```powershell
python TOOLS/exr_callgraph.py
python TOOLS/exr_decompile_at.py <hex-address>
python TOOLS/exr_disasm_fn.py <hex-address>
```

`ANALYSIS/exr_callgraph.txt` lists functions and their callees. Remaining targets include full attack-strength/combo handling, MRS clock/control behavior and integration, AI behavior, round timer, exact movement timing, and visual validation of CL2 box placement. MRS header/event layout, SOS register parameters and sound-quality settings are documented in [the audio investigation](13_audio_investigation.md); attack filtering and jump physics are verified in document 14.
