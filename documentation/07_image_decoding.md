# 07 — Image decoding and verification

Addresses refer to the DOS `RISE2_EXR` image loaded at 0x10000. The importer can now decode the original game's image resources; this page records the format evidence and the limits of the exports. The Director's Cut has additional material, particularly ANI files, which remains to be decoded.

## GGF: palettes, LZW, and dimensions

Of 188 GGF files in the original installation, 185 decode and round-trip exactly. The three exceptional files are malformed or use a different layout. The high-resolution robot sheets comprise 91 images at 400×200 and 93 at 800×400; A6K is 640×400. Pixel indices are stored linearly after decompression. The 28 high-resolution RBT banks yield 9,232 frames, while the RB4 low-resolution banks yield 9,234.

When the compression flag is set, a 768-byte RGB palette follows the header. The LZW stream uses LSB-first codes, starts at 9 bits, grows to 12 bits, reserves `0x100` for CLEAR and `0x101` for EOI, and has one zero padding byte. The implementation reproduces the DOS decompressor, including its dictionary overlap behavior for A6K at `0x3c781–0x3c795` with relocated output buffers. `FUN_3c731` crops 80 pixels, then 640 pixels, then 160 pixels from an 800-pixel source row.

The A6B/A6G files carry the first 495 bytes of the AGK palette; A6M contains a repeated 494-byte fragment. They cannot be treated as ordinary GGF images.

## ANL/ANR animation banks

An ANL file begins with a u16 count followed by u32 offsets. The matching ANR file stores sparse frame spans terminated by `0xffff`. An earlier AIP interpretation was incorrect: AIP is read at `0x62ad8` and has a different role.

| Span word `t` | Extra data | Decoded position and length |
|---|---|---|
| `t == 0xffff` | none | End of frame |
| `(t & 0xc000) == 0` | bytes `a,b` | `x = a + ((t & 3) << 8)`; `y = b + ((t & 4) << 6)`; `n = t >> 3` |
| `(t & 0x8000) == 0`, bit 14 set | byte `a` | `x = a + ((t & 3) << 8)`; `y = previous_y + 1 + ((t & 0x3fff) >> 12)`; `n = (t >> 2) & 0x3ff` |
| Bit 15 set, `(t & 0x7000) == 0` | byte `a` | Same `x` and `y` as the preceding case; `n = (t >> 2) & 0x3ff` |
| Bit 15 set, `(t & 0x7000) != 0` | none | `x = previous_end + ((t & 0x7fff) >> 12)`; same `y`; `n = t & 0x3ff` |

Each span is followed by `n` palette indices, then `previous_end = x + n`. Pixels outside spans are transparent. Palette index 0 inside a span is opaque. Frame origins must be preserved when placing sprites; cropping every frame to its own bounds causes visible jitter.

## Palettes and atlases

`FUN_1a473` loads 70 colors per player from an `R?.PAL` file, even though 80 entries are present. `FUN_20dda` offsets the second player's indices by 70. Shared effect colors occupy 203–239 and 253. Some opposing-robot colors come from `RBTP`/`RBTN`. For a single-robot preview, indices 0–69 use the robot palette, 70–139 repeat it, and 140–255 use the arena AG palette. `VSFACE` contains 56 portraits; its 70-color blocks follow the ABC…Z01 roster order. `FUN_1a67e` applies in-game color changes and does not define the base export palette.

The exporter writes RGBA atlas pages and manifests, as well as `indices_*.png` images that retain palette indices and alpha. The original 28 robots produce 139 atlas pages. The 126 ANR banks contain 20,969 frames, including 1,199 empty frames. The gallery is a visual browser; its loop speed is not evidence of the DOS game's animation timing.

## Verification

- 185 GGF files decoded with exact input consumption and round trips.
- 20,969 ANR frames stay within their ANL next-frame bounds.
- 18,522 atlas rectangles match their source frame hashes.
- 456 frames sampled across 126 banks match x86 execution in Unicorn.
- A4A, AG0, and VS GGF output matches the x86 code exactly; A6K matches with relocated buffers.
- Nine codec regression tests cover the decoded formats.

Typical commands:

```powershell
python TOOLS/extract_ggf.py
python TOOLS/extract_anr.py
python TOOLS/extract_anr.py --banks RBTA --individual --output EXTRACTED/single_bank
python TOOLS/build_image_gallery.py
python TOOLS/test_image_codecs.py
python TOOLS/verify_images.py
python -m http.server 8762 --bind 127.0.0.1 --directory EXTRACTED
```

For optional x86 comparison, install `unicorn==2.1.4` into `ANALYSIS/verification_deps` and run `python TOOLS/verify_x86_images.py`.
