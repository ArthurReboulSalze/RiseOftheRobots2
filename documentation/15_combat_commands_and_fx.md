# 15 — Robot commands, original combat FX and finishings

## Playable implementation

The port reads each selected robot's commands and visual scripts from its own MVS bank. The six configured attack keys now retain light, medium and heavy strength. Attached effects, travelling projectiles and their impact animations use the original sprites. Direct hits apply the CL2 body multiplier and reaction region; signed attack bytes can select specific victim states. Backward input guards against a nearby attack or projectile. The round now ends, offers a finishing window, plays available finishing/defeat/victory sequences, and supports a rematch.

| Control | Behavior |
|---|---|
| Configured punch keys 1/2/3 | Light / medium / heavy punch |
| Configured kick keys 1/2/3 | Light / medium / heavy kick |
| Forward + attack within 61 logical pixels | Original close-range movement 72/73 |
| Backward against an incoming attack | Guard; down + backward uses crouching guard |
| F1 | Pause and display both selected robots' command lists and configured keys |
| F2 | Switch player 2 between basic CPU sparring and its configured keyboard |
| Enter after the result | Rematch |
| Escape | Existing pause menu; quit match returns to selection |

The list uses directions relative to the opponent: F/B, U/D, DF/DB, UF/UB. P/K denote punch/kick; N denotes release and `*` one arbitrary history entry. Super commands require a full 24-unit meter. `LOCKED` entries are stolen powers, not ordinary specials. `NO DATA` marks an empty action, `NO INPUT` an unproducible script input, and `ORDER` a command masked by an earlier normal command. Original command order is preserved. Movement numbers are identifiers, not invented move names.

**OPTIONS → EASY FINISHINGS** provides an optional attack-button shortcut after KO. It lists the selected robot's available even 48–58 source actions, sorts them by movement ID and connects light/medium/heavy punch and kick consistently. If there is only one, any attack button selects it. A fresh press is required; holding the knockout strike does not trigger a finishing automatically. F1 shows the shortcut. Original command inputs remain available. This assist bypasses command entry and calculates placement from the source finishing's collision geometry; it does not invent missing actions or implement the remaining cinematic callbacks. Details are in [documents 16](16_options_filters_and_fx.md) and [17](17_finishings_voices_and_player.md).

## Input strength and command matching

Addresses below refer to the analysed older `RISE2.EXR`; executable-dependent table locations are mapped separately for Director's Cut.

`FUN_197b6` maps six attack buttons to two attack groups and a separate strength:

| Field | Address for player 0 | Meaning |
|---|---|---|
| Current strength | `0x66234` | 30, 60 or 90 |
| Strength tier | `0x66235` | 0, 1 or 2 |
| Saved movement strength | `0x66237` | Copied when an input changes movement |
| Relative input mask | `0x66238` | P=1, F=2, B=4, U=8, D=16, K=32 |
| Command history | `0x685dc` | 16 entries, newest first |

Player fields add stride `0x97`; the second history adds 16. Holding any attack suppresses further attacks from both groups until release. Directions are swapped when facing left. `FUN_21baf` selects an animation/displacement stream using `(saved_strength*3+3)/maximum_energy`, clamped to 0–2. At maximum energy 120, the three strengths select three distinct streams. Attached visuals use `saved_strength*3/(maximum_energy+3)` in `FUN_24819`.

The history writer at `0x161c0` strips directions from attack events and stores changes. After five unchanged history updates it inserts `0xf0`. `FUN_165a4` clears the history and applies an eight-update lock after an accepted command. The port retains input changes sampled between simulation ticks, runs the history clock at the recovered 25 Hz logical rate and buffers a press through hit pause. The native IRQ invokes the history writer with this same counter; [document 08](08_engine.md) records the timing verification.

`FUN_234fa` scans the command section in file order, then falls back to ordinary transition masks. Input bytes precede `0xff` and a target movement byte; an empty `0xff` ends the list. `0xfe` consumes exactly one history entry. It does not mean “skip an arbitrary number of inputs”. Scripts are stored newest first; the display reverses them into execution order.

`FUN_2695b` gates commands by ground/air condition, available projectile slots, current action and hit-confirm cancellation. It rejects movements 72/73 and normally prevents interrupting an attack after its first two frames. STS `0x20` blocks scripted interruption. Movements 90–95 require and consume individual stolen-power bits. Movement 89 additionally requires a grounded, nearby, weakened opponent of native robot ID 6 or 18. The port implements these gates; progression and awarding stolen powers remain separate work. Native IDs are A–Z followed by 0–3; the displayed roster has now been corrected to that same order.

Some data contains impossible input bytes or ordered-prefix conflicts. Original-instruction execution of RBTY confirms that its command 81 wins over command 88, and command 84 wins over finishing command 50. The port reports these conflicts rather than silently reordering the bank. Whether the original game has an additional path to expose those actions remains unresolved.

## MVS effect directory

The MVS header's dword at `+8` points to commands. The dword at `+4` points to **96 twenty-byte visual records**, not AIP CPU data:

| Record offset | Pointer meaning |
|---|---|
| `+0`, `+4`, `+8` | Attached visual scripts for three strengths |
| `+12` | Projectile motion/animation script |
| `+16` | Projectile impact script |

The native loader stores its movement-directory base twelve bytes into the file. `FUN_24149` accesses the projectile pair through this shifted base; `FUN_24819` subtracts twelve for the attached triplet. Missing that distinction misidentifies the pointer order.

Each effect instruction contains four signed 16-bit words: image, horizontal delta, vertical delta, flags. Image 1000 is an invisible delay that still moves; -999 terminates, sometimes as a two-byte stop between script pointers. Other negative images jump backwards by that many eight-byte records. Flag `0x02` selects EXTRA sprites/CL2; other instructions select the owner's robot bank. Projectiles normally disappear when the owner leaves a special action unless flag `0x01` allows persistence. Their direction is captured on creation; crossing cannot reverse an existing projectile.

`FUN_244eb` advances projectile motion, `FUN_24926` advances attached visuals, and `FUN_247ca` switches a projectile to its impact script after a hit. There are three projectile slots per player. The port preserves these limits, invisible delays, backwards loops, impact switching and captured directions. It permits a finishing projectile to complete during the finishing demonstration, including movements below 80.

## EXTRA sprites and impacts

EXTRA contains 280 frames. Its PAL contains an eight-byte header followed by **768 eight-bit RGB bytes**; the robot PAL format is different. The placeholder green colors in arena palettes cannot color these effects correctly. `extract_anr.py` now exports EXTRA with its own palette.

EXTRA frame indices are direct. Robot MVS images still select atlas frame `image+1`. `FUN_191be` and `FUN_192da` send both shared and robot effects to the same native blitter with owner-relative or projectile-world coordinates. Attached visuals and projectiles therefore share the owner's authored canvas projection and sprite scale; treating EXTRA as a separate 320/200 anchor put shots near the floor. Projectile collision boxes use the same projection. Ordinary contact impacts retain their existing overlap-point placement. Embedded robot effects now also receive EXTRA.PAL colors at indices 203–239: the arena's green reserves are not their true colors. See document 16 for the Cyborg reproduction and palette limits.

`FUN_39fde` and `FUN_3a0d2` drive four impact slots from four executable scripts. `extract_combat.py` exports those scripts, sixteen particle sequences, per-robot super strengths and four body-reaction states. It supports the two currently analysed executable hashes and refuses unknown table layouts.

The runtime renders original strike/guard impacts, attached effects, projectiles and projectile explosions. Periodic damage smoke, the sixteen particle emitters, screen distortion and some robot-specific finishing callbacks are extracted or identified but not fully integrated. They must not be confused with the impact FX already running.

## Damage and reactions

`FUN_3885e` selects the deepest horizontal overlap between an attack and body box. Body byte `+4` is the reaction region; byte `+5` is a damage multiplier, with zero treated as one for damage. Attack byte `+4` is signed. A negative value selects victim movement `-value`; `FUN_3a2ea` supplies replacement base damage, normally 4, with three robot/action exceptions at 12.

For a normal direct strike, `FUN_38b72` calculates:

```text
max(1, (signed16(base_damage * body_multiplier * attack_strength) * attacker_stat) >> 13)
```

Ordinary strength is 30/60/90. Specials normally use 80, or 50 when STS `0x40` applies; movement 88 reads its robot's executable table. These values describe the attacker, not the victim's defense. The port currently uses the neutral initialization stat 100; original difficulty and per-round tuning are not yet connected. Standing guard only reduces direct damage for multiplier-zero body boxes; crouching guard has an additional attacker-state condition. Reduction is one eighth, minimum one.

`FUN_39b78` uses projectile strength 40 and default scale 256, shifting by 15 for ordinary damage and 17 for guard damage. A projectile changes to its impact script once and cannot keep dealing damage while exploding. Direct hits grant hit-confirm cancellation except for STS `0x40`; ordinary strikes are latched until the next movement. That flag allows repeated special hits. Super charge comes from a first special/projectile hit, +2 when guarded or +4 otherwise, capped at 24; a super does not charge itself.

The port chooses reaction 11–14 from the exported body-region table, crouching reaction 27, airborne reaction 43, or the signed CL2 override. It includes ordinary hit pause and decreasing pushback. Finishing demonstrations re-arm separated active phases so their later CL2 death reactions can execute. Precise native stun counters, knockback thresholds, linked grabs and super-dependent displacement require further DOS comparison.

## End of round

`FUN_15403` provides a separate post-round phase. It recognizes ordinary defeat state 15, puts the defeated robot into stagger state 75, suppresses that player's input, enables finishing mode and allows the winner to continue. Even movements 48–58 are finishing actions; odd victim movements 49–59 are finishing reactions. Their relationship is carried by collision data, not a universal guessed `attacker_move+1` rule.

The port exposes a 200-tick finishing window, now eight seconds at the recovered 25 Hz cadence. A valid command plays its source sequence and effects at the same logical rate as combat. Later signed CL2 reactions can drive the victim's finishing animation. A normal extra strike or an expired window leads to defeat 15 and victory 64. The result settles when the ending animations finish, with a bounded fallback for source loops. Enter starts a clean rematch. Exact round transitions, boss/unlock restrictions, linked cinematics, stolen-power awards and some callbacks are still approximations of the complete DOS round manager. This is an initial playable integration, not a claim that every robot's full finishing presentation is reproduced.

The optional single-button assist now calculates separation from the finishing's signed death collision and the victim's body box. Cyborg movement 58 needs the later image-333 box (signed -59), which missed at the previous universal 100-pixel gap. Against Surpressor the assist chooses 226 logical pixels and reaches victim state 59, including near walls. [Document 17](17_finishings_voices_and_player.md) distinguishes this verified collision placement from the unresolved indexing in DOS's separate finishing-range gate. VICT ending movies are now available through PLAYER; they are not automatically assigned to finishing actions.

## Refresh and verification

Existing imports need the metadata, EXTRA atlas and corrected robot atlases; a full import now does this automatically. To refresh a private profile without re-ripping its game or audio:

```powershell
python TOOLS/extract_mvs.py --source LOCAL/my-game/game --output LOCAL/my-game/EXTRACTED/data/mvs
python TOOLS/extract_anr.py --source LOCAL/my-game/game --output LOCAL/my-game/EXTRACTED/sprites
python TOOLS/extract_combat.py --source LOCAL/my-game/game --output LOCAL/my-game/EXTRACTED/data/combat.json
```

The new `combat_rules` CTest uses synthetic data for strengths, mirrored commands, wildcard history, super/stolen/finishing gates, script loops, damage/body reactions, held-hit protection, KO and a signed finishing reaction. Existing fighter and audio tests remain. Python fixtures cover command boundaries and big-/little-endian effect scripts, including short stops and invalid loops.

`TOOLS/verify_x86_combat.py` executes the supplied older EXR's command matcher, real gates and projectile interpreter with relocated private MVS/STS data. RBTF exercises fourteen command entries and twelve projectile ticks; RBTY verifies original prefix priority conflicts. No executable instructions or game scripts are included in the tool. `rotr2_asset_smoke` checks 306 executable commands in the 28-robot profile and 326 in Director's Cut's 30-robot profile, plus all FX frame references; respectively 12 and 14 empty, impossible or shadowed entries remain identified. The checks also caught and corrected selection cycling beyond the older edition's roster, and missing default controls when a profile has no CFG. `rotr2_headless` shares the runtime combat class and verifies two held strikes, a full jump, a real EXTRA projectile hit, Cyborg's uppercut in both directions, and PRIME 8's command-driven and single-button finishing with a CL2 death reaction. The bank was incorrectly labelled WAR before the roster correction. Generated captures and reports stay private. Interactive feel and complete finishing presentation still need user playtesting.
