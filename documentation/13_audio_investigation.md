# 13 — Audio quality and sequencing investigation

Verified September 26, 2026 against both supplied DOS executables and Director's Cut Disc 1. This distinguishes stored PCM, per-note playback speed and mixer output quality. All original data and analysis reports remain private.

## Sound-quality setting

The menu text in `OPTIONS.TXT` defines five SOUND QUALITY choices. In the older build, `FUN_2cbc1` reads the selected value from the options buffer at `DAT_62868 + 0xb58` and writes the u16 index at `0x62e80`. `FUN_3a3e8`, specifically `0x3a481–0x3a494`, indexes the u16 rate table at `0x62e74` and writes the result into the SOS driver structure at `0x62c7c`. It requests one channel and eight bits.

| Index | Menu quality | Requested mixer rate |
|---:|---|---:|
| 0 | Low | 5,000 Hz |
| 1 | Mid-low | 6,500 Hz |
| 2 | Mid | 8,000 Hz |
| 3 | Mid-high | 9,500 Hz |
| 4 | High | 11,025 Hz |

Director's Cut has the same table at `0x62e78`. Its chain is `FUN_2ce6d` → index `0x62e84` → `FUN_3a8c3`, writing the requested rate at `0x62c80`. The sample format remains eight-bit mono. These paths configure the driver; they do not select another MRW directory or decode a compressed high-quality sample bank.

Unicorn execution confirms all five requested rates in both builds. A hardware driver, Windows or DOSBox can additionally convert output to its device rate, but that does not establish a separate native 44.1 kHz sample source. This finding is limited to the two supplied DOS EXR builds, rather than all console/Windows releases.

## Sample lookup and playback parameters

The older `FUN_3c680` forwards register parameters to `FUN_3a792`; Director's Cut uses `FUN_3ac6d` → `FUN_3acb0`.

| Register | Meaning |
|---|---|
| EAX low word | MRW sample index |
| EDX low word | Volume, nominal full scale `0x7fff` |
| EBX low word | Loaded MRW bank number |
| ECX | Playback-rate multiplier in 16.16 fixed point |

The two extra stack words are forwarded but unused by the direct sample routine. The bank begins with u16 count and eight-byte directory entries `(u32 absolute_offset, u32 byte_length)`. The pointer passed to SOS is `bank_base + absolute_offset`. No decompression occurs in this path.

The working SOS sample structure begins at `0x62d84` in the older build and `0x62d88` in Director's Cut. Relevant relative fields are:

| Offset | Value/meaning |
|---|---|
| `+0x00` | PCM pointer |
| `+0x0c` | PCM byte length |
| `+0x2c` | Left/right volume packed as two u16 values |
| `+0x30` | Loop count; zero for direct one-shot effects |
| `+0x34` | Playback Hz, `(ECX * 11025) >> 16` |
| `+0x38` | Sample bits, initialized to 8 |
| `+0x3c` | Sample channels, initialized to 1 |
| `+0x40` | Fixed flags `0x8000`; no additional flag interpretation claimed |
| `+0x44` | Center pan `0x8000` |

Volume is first scaled by the current master setting: `EDX * master / 48`. The older master word is at `0x70450`; Director's Cut uses `0x7044e`. The direct-hit call at `0x39b54–0x39b6a` requests sample 15, attacker bank + 1, volume `0x2000`, and rate multiplier `0x10000`. Thus its native playback rate is 11,025 Hz, not a low pitch of `0x2000`. The earlier journal attribution EDX=pitch / EBX=pan / ECX=volume was incorrect.

`TOOLS/verify_x86_audio.py` executes the original lookup and rate/volume calculations until the SOS start boundary. For R0 sample 15, both builds produce length 6,929 and packed volume `0x20002000`. Changing only ECX to `0x20000` doubles playback to 22,050 Hz while retaining exactly the same PCM pointer and length. This is a pitch change, not a higher-resolution replacement.

## Source comparison

The two older source folders have 69 MRW banks; all 69 are byte-identical. Director's Cut contains these same 69 files unchanged, plus R2 and R3, for 71 banks. Its complete Disc 1 data filesystem has 1,128 files, including 1,122 in RISE2; no extra WAV/VOC or alternative effect bank was found outside that directory. The audio tracks on the same disc provide the 44.1 kHz stereo soundtrack, not a second set of fighter effects.

The original WAV exports preserve unsigned eight-bit mono data with midpoint `0x80`. A directory named `mrw_hq` from an earlier session contains converted copies, not newly discovered higher-quality sources. Some native clips already reach the eight-bit limits; conversion cannot recover their clipped peaks or missing frequencies.

## Modern playback correction

The port now converts effect WAVs to its actual SDL_mixer device rate using a 32-tap Blackman-windowed sinc filter. The normal output is 44.1 kHz, 16-bit stereo. Native mono goes equally to both channels. Impact gain is restored to `0x2000 / 0x7fff`, applied before conversion so reconstructed peaks retain headroom. Original WAVs are neither overwritten nor relabelled as high-resolution recordings.

Synthetic audio tests check duration, silence, original tone level and frequency, stereo consistency, gain, suppression of resampling images, downsampling alias rejection, and actual SDL_mixer loading/playback/freeing with a dummy audio device. These establish conversion behavior, not subjective equivalence to DOS playback. Listening comparison remains necessary.

## MRS music and fighter sequences

Digital music exists as MGA–MGF MRS/MRW pairs. These are custom note/sample sequences, not standard MIDI/HMI/HMP files. BGA–BGZ provide arena ambience. Fighter R-slot MRS files also carry sound sequences; the music-only interpretation in the previous audit was incomplete.

`FUN_3be83` establishes the layout below, with C = channel count and S = sequence count. Each table is channel-major: entry `channel*S + sequence`.

| File position | Contents |
|---|---|
| `0` | u16 C |
| `2` | u16 S |
| `4` | C u16 stream sizes; a zero-sized single-channel bank uses the remaining file |
| `4 + 2*C` | C u16 instrument selectors |
| `4 + 4*C` | C×S u16 stream-relative sequence offsets, `0xffff` disables a channel |
| `4 + C*(4 + 2*S)` | C×S u16 initial delays |
| `4 + C*(4 + 4*S)` | C×S u16 trigger identifiers |
| `4 + C*(4 + 6*S)` | Channel streams, concatenated according to their sizes |

For R0, C=1 and S=48, so streams begin at `0x128`, not `0x1e4`. Its first event is `00 00 00 7f ff ff`: delay 0, sample 0, volume 127, then end marker. MGA has C=11, S=9 and begins its streams at `0x282`; MGB has C=13, S=2 and begins at `0xd4`.

`FUN_3bd8e` reads a little-endian u16 delay. `0xffff` ends a stream. Positive delays at least 30001 rewind the cursor by `4*delay - 119998` before reading the next delay. Ordinary events then use two bytes: sample/note and volume. Selector 0 takes the first byte as the sample index with normal multiplier `0x10000`; otherwise it selects sample `instrument + 7` and reads the note multiplier from `0x62e34 + 4*note`. Note 84 is `0x10000`. `FUN_3b137` converts the volume byte to the SOS scale by shifting it left eight bits.

The callback at `0x3ae71` processes zero-delay events, dispatches samples through `FUN_3b137` → `FUN_3a792`, and decrements positive delays once per callback. `0xff` switches sequence; `0xfe` conditionally switches it according to `DAT_70618`. Opcode `0x7f` and fighter sample 14 have additional shadow-bank behavior. `FUN_3b96a` initializes a chosen sequence from its offset/delay tables. Fighter state handling at `FUN_23031` uses `FUN_3c50e` to find trigger identifiers, then queues a sequence through `FUN_3c2e9/3c395`. This is separate from the direct successful-hit sample.

The remaining work is to trace callback registration and its effective tick frequency, reproduce control/shadow-bank behavior and loops, then integrate a sequencer into the port. Imported CD tracks already play; MRS digital music is preserved and structurally identified, but is not yet rendered into complete tracks or played by the port.

## Reproduction

```powershell
python TOOLS/verify_x86_audio.py --source LOCAL/my-game/game --output LOCAL/my-game/ANALYSIS/audio_x86.json
cmake --build PORT/build --config Release
ctest --test-dir PORT/build -C Release --output-on-failure
```

The x86 verifier needs the optional Unicorn dependency documented in its header and recognises the two analysed executable hashes. Other builds require mapping their own addresses first. Ghidra projects, generated PCM and verification reports are excluded from the public repository.
