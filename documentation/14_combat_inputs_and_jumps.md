# 14 — Attack inputs, jump physics and crossing

This investigation fixes held attacks repeating, jump animations playing without vertical movement, and repeated direction changes when crossing an opponent. Addresses below belong to the older DOS EXR loaded at `0x10000`, SHA-256 `213ba86c2292cb3203f87826c030fbdb9d4c77c3a8c27d69fce230a53dc4241b`. Ghidra decompilation was checked against assembly; `TOOLS/verify_x86_fighter.py` independently executes the original input, vertical-physics and crossing instructions using the user's private executable.

## Physical buttons and MVS masks

`FUN_197b6` reads the ten configured keys per player from `DAT_5097a`. It keeps the previous raw keys at `DAT_67a70`. If ANY attack key was held in the previous sample, it suppresses ALL six attack keys for that player. Switching directly from punch to kick therefore requires releasing the attack group first. Direction keys remain held inputs.

The builder maps punches to `0x01`, kicks to `0x20`, up to `0x08` and down to `0x10`. Initially, screen right is `0x02` and screen left `0x04`; the end of the function swaps those two bits for a fighter facing left. Consequently, **MVS `0x02` means forward and `0x04` backward**, whereas the port's public inputs retain screen directions. The earlier journal statement that directions were never relative to facing was incomplete.

`FUN_234fa` compares masks containing `0x21` against attack bits alone. `0x21` is the attack group, not walk bits. For example, a punch can start while holding forward. Attack strength is stored separately by the DOS input builder; the port still collapses the three punch buttons and three kick buttons to their respective basic actions.

Confirmed movement IDs in the normal RBT tables:

| Movement | Meaning |
|---|---|
| `0` | Standing idle |
| `2`, `3` | Backward, forward walking |
| `7` → `16` | Crouch entry → crouch hold |
| `8`, `9` | Standing punch, kick |
| `23` | Rise from crouch |
| `24`, `25` | Crouching attacks |
| `32`, `34`, `35` | Neutral, backward, forward jumps |
| `37`–`42` | Airborne attacks |

Do not substitute guessed state lists for the bank's control bytes. In particular, movements 8 and 9 are attacks and must not loop. RBT0 and RBTT do not contain a normal idle-to-jump transition in either supplied edition; their neutral-jump descriptor also lacks the normal impulse flag. The port respects those data restrictions instead of creating a jump unsupported by the source.

## STS records and MVS completion

`FUN_23e97` reads `0x540` bytes from `RBT?.STS` to `0x67aa4 + player*0x540`. That is 96 records of 14 bytes. The player pointer at `0x6625c + player*0x97` selects this table. These state records are **STS**, not the similarly sized on-disk AIP records used for CPU action scripts.

Properties used in this correction:

| STS byte | Meaning / use |
|---|---|
| `+0` | Ground/air condition used when accepting scripted actions |
| `+1` | Signed action type; values above 1 participate in attack locks |
| `+4` | Unsigned gravity byte; `FUN_21baf` computes `(byte * 3) / 4` |
| `+6` | State flags; `0x10` permits replacing an impulse while airborne; `0x20` blocks scripted interruption |

`extract_mvs.py` exports these four properties under each movement's `state` key. The importer requires the base robots' STS sidecars. Previous exports can be refreshed without reconverting sprites or audio:

```powershell
python TOOLS/extract_mvs.py --source LOCAL/my-profile/game --output LOCAL/my-profile/EXTRACTED/data/mvs
```

The MVS control byte at descriptor `+0x1c` is handled by `FUN_21baf`:

| Bit | DOS behavior |
|---|---|
| `0x01` | Hold the last sequence frame at the end marker |
| `0x02` | Automatic transition to byte `+0x1d`; wait if airborne or still rising |
| `0x04` | Clear current attack-strength byte; saved movement strength remains separate |
| `0x08` | Resume the sequence at byte `+0x1e` |
| `0x10` | Count completions; after byte `+0x1f` repeats, use automatic target/resume |
| `0x20` | Apply horizontal steering through `FUN_23138` |
| `0x40` | Initialize signed vertical impulse from byte `+0x1f` and gravity from STS |
| `0x80` | Suppress immediate automatic transition in the landing helper |

The port follows hold, automatic return, resume and repeat controls, plus the vertical impulse and landing rules. It keeps the final visible air frame until landing rather than rendering the end marker or restarting the whole jump. [Document 15](15_combat_commands_and_fx.md) adds scripted commands, three attack strengths, horizontal steering and hit-confirm gates, with their remaining limits.

## Vertical motion and landing

`FUN_21baf` initializes velocity once on movement entry: `signed_byte(MVS[+0x1f]) * 256`. It only replaces that velocity on the ground or when STS flag `0x10` permits it. Gravity is `unsigned_byte(STS[+4]) * 3 / 4`. Treating gravity byte `0xf8` as -8 would prevent the fighter from returning to the ground.

`FUN_231ac` updates these player-0 fields; player 1 adds stride `0x97`:

| Address | Field |
|---|---|
| `0x66214` | Y position |
| `0x6621c` | Signed 8.8 vertical velocity |
| `0x66222` | Fractional displacement accumulator |
| `0x66252` | Fixed-point gravity |
| `0x66254` | Ground Y |

Each combat tick adds twice gravity to velocity, adds velocity to the accumulator, adds `2 * trunc(accumulator / 256)` to Y, then retains the accumulator's low byte. At/below the ground, Y clamps to ground Y and velocity becomes zero. When MVS bit `0x02` is set and `0x80` clear, landing selects the automatic target and resume byte. This is distinct from merely finishing the animation sequence.

For RBTF movement 32, the impulse byte is `0xed` (-19), the STS gravity byte `0xf8` (248), and fixed gravity 186. With ground Y=312, the original instructions produce:

```text
278 248 220 196 174 156 140 128 118 110 106 106 106
108 114 122 134 148 166 186 208 234 264 294 312
```

The port's synthetic fighter test checks that measured trajectory, airborne collision offsets and lack of a second impulse during an air attack. It also checks held attacks, release/repress, switching attack buttons, a tap sampled between simulation ticks, facing reversal and no repeated jump when up stays held. Single-press jump behavior is an explicit port input rule; the DOS builder itself leaves up held.

## Crossing and orientation

The earlier port changed facing both before and after motion, without changing the movement state. Crossing during forward jump 35 therefore flipped the sign of its displacement, sent the fighter back across the opponent, and flipped it again on the next tick. The visible mirroring and horizontal oscillation had the same cause.

`FUN_2163a` calls `FUN_25615` once at the beginning of the combat tick. In normal combat, the high word of `DAT_703b2` is -1. The routine permits turning in movement IDs `0, 2, 3, 6, 16, 18, 22, 26, 32, 33, 34, 35, 60, 61, 62, 74, 75, 76, 77, 79`; ordinary standing and airborne attacks keep their orientation until completion. When an eligible fighter crosses:

| Current movement | DOS turning behavior |
|---|---|
| `34`, `35` | Swap backward/forward jumps `34↔35` together with facing |
| `32`, `75` | Change facing without replacing the movement |
| `16` | Enter crouching turn `69` (`0x45`) |
| Other eligible states | Enter standing turn `68` (`0x44`) |

For paired jumps, `FUN_26321` only clamps the current sequence frame to the target sequence's last visible frame when necessary. It does **not** restart the animation. The port preserves sequence progress, vertical velocity, fractional displacement, movement initialization and the hit latch when exchanging 34/35. The opposite signed horizontal streams preserve travel in screen coordinates after facing changes.

At equal X, the DOS routine flips player 0 only when both fighters are exactly at ground Y and face the same direction. Airborne equality leaves facing alone. Both the interactive fight and headless simulation now share these rules and apply them only before inputs and motion.

`TOOLS/verify_x86_fighter.py` executes the original `FUN_25615` and `FUN_26321` with a relocated private RBTF bank. Sixteen cases cover both directions, standing/crouching turns, locked attacks, neutral jumps and paired jumps, including an overlong frame index. The observed state, facing, frame, Y and vertical velocity match the documented behavior. Additional linked-state pairs `26↔33` and `76↔77` belong to a different DOS link mode; they are not applied unconditionally. The original pushback override and linked combat mode remain unported.

## Verification and limits

The Release build and both CTest checks passed. Synthetic fighter checks cover ground crossing, both diagonal-jump directions, unchanged jump trajectories, locked attacks, frame clamping and stable orientation at equal X. The private 30-robot Director's Cut profile passed the real C++ loaders, held punch/kick checks and, for robots with supported jumps, diagonal crossing in both directions followed by landing. The headless fight verifies two held attack presses produce two hits, and RBTF rises then lands while up remains held. Python import/LE/STS fixtures contain only synthetic data.

The simulation remains at the user-approved 15 Hz. The DOS trajectory arithmetic is verified; exact original frame pacing, move strength, combo rules, hitstun, push boxes and gameplay feel still need direct DOS comparison and playtesting. No game bytes, images, PCM, Ghidra outputs or generated profiles are included in Git.
