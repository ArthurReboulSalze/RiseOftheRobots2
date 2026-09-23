"""ANL/ANR sprite decoder translated from RISE2.EXR, 0x1c9d0..0x1ca6d.

ANL: u16 frame count followed by u32 little-endian ANR offsets.
ANR: variable-length horizontal pixel spans, terminated by u16 0xffff.
Pixels outside the spans are transparent, including when index 0 is opaque.
"""

from dataclasses import dataclass
from pathlib import Path
import struct
from PIL import Image


class SpriteFormatError(ValueError):
    pass


@dataclass(frozen=True, slots=True)
class Span:
    x: int
    y: int
    pixels: bytes


@dataclass(frozen=True, slots=True)
class Frame:
    offset: int
    end: int
    spans: tuple[Span, ...]

    @property
    def bbox(self):
        """Exclusive right/bottom coordinates, or None for an empty frame."""
        if not self.spans:
            return None
        return (min(s.x for s in self.spans), min(s.y for s in self.spans),
                max(s.x + len(s.pixels) for s in self.spans), max(s.y for s in self.spans) + 1)


def decode_frame(data, start=0, end=None):
    end = len(data) if end is None else end
    if not 0 <= start < end <= len(data):
        raise SpriteFormatError("Limites de frame invalides")
    pos = start
    previous_y = previous_end = None
    spans = []

    def need(count):
        if pos + count > end:
            raise SpriteFormatError(f"Segment tronqué à 0x{pos:x}")

    while pos < end:
        need(2)
        token, = struct.unpack_from("<H", data, pos)
        pos += 2
        if token == 0xffff:
            if pos != end:
                raise SpriteFormatError(f"Fin prématurée à 0x{pos:x}, attendue 0x{end:x}")
            return Frame(start, end, tuple(spans))
        if not token & 0xc000:
            need(2)
            x = data[pos] | ((token & 3) << 8)
            y = data[pos + 1] | ((token & 4) << 6)
            pos += 2
            length = token >> 3
        else:
            if previous_y is None:
                raise SpriteFormatError("Premier segment relatif sans coordonnées absolues")
            if not token & 0x8000:
                y = previous_y + 1 + ((token & 0x3fff) >> 12)
                need(1)
                x = data[pos] | ((token & 3) << 8)
                pos += 1
                length = (token >> 2) & 0x3ff
            else:
                y = previous_y
                if token & 0x7000:
                    x = previous_end + ((token & 0x7fff) >> 12)
                    length = token & 0x3ff
                else:
                    need(1)
                    x = data[pos] | ((token & 3) << 8)
                    pos += 1
                    length = (token >> 2) & 0x3ff
        if length == 0:
            raise SpriteFormatError(f"Segment vide à 0x{pos:x}")
        if x + length > 4096 or y >= 4096:
            raise SpriteFormatError(f"Coordonnées excessives : {x}, {y}, {length}")
        need(length)
        spans.append(Span(x, y, bytes(data[pos:pos + length])))
        pos += length
        previous_y, previous_end = y, x + length
    raise SpriteFormatError("Terminateur 0xffff manquant")


def read_bank(path):
    path = Path(path)
    if path.suffix.upper() not in (".ANL", ".ANR"):
        raise SpriteFormatError("Une paire ANL/ANR est nécessaire")
    index = path.with_suffix(".ANL").read_bytes()
    data = path.with_suffix(".ANR").read_bytes()
    if len(index) < 2:
        raise SpriteFormatError("Index ANL tronqué")
    count, = struct.unpack_from("<H", index)
    if not count or len(index) != 2 + count * 4:
        raise SpriteFormatError("Taille de l'index ANL incompatible avec son compteur")
    offsets = struct.unpack_from(f"<{count}I", index, 2) + (len(data),)
    if offsets[0] != 0 or any(a >= b for a, b in zip(offsets, offsets[1:])):
        raise SpriteFormatError("Offsets ANL non croissants ou premier offset non nul")
    frames = []
    for i, (start, end) in enumerate(zip(offsets, offsets[1:])):
        try:
            frames.append(decode_frame(data, start, end))
        except SpriteFormatError as exc:
            raise SpriteFormatError(f"{path.stem}, frame {i}: {exc}") from exc
    return frames


def indexed_frame(frame):
    """Return cropped index and opacity images without assigning any colors."""
    bbox = frame.bbox
    if bbox is None:
        return Image.new("L", (1, 1)), Image.new("L", (1, 1))
    left, top, right, bottom = bbox
    w, h = right-left, bottom-top
    pixels, alpha = bytearray(w*h), bytearray(w*h)
    for span in frame.spans:
        start = (span.y-top)*w + span.x-left
        n = len(span.pixels)
        pixels[start:start+n] = span.pixels
        alpha[start:start+n] = b"\xff"*n
    return Image.frombytes("L", (w, h), bytes(pixels)), Image.frombytes("L", (w, h), bytes(alpha))


def render_frame(frame, palette):
    """Return a cropped RGBA image; retain frame.bbox as its drawing origin."""
    if len(palette) != 768:
        raise ValueError("Palette RGB de 256 couleurs requise")
    indices, alpha = indexed_frame(frame)
    image = Image.frombytes("P", indices.size, indices.tobytes())
    image.putpalette(palette)
    image = image.convert("RGBA")
    image.putalpha(alpha)
    return image
