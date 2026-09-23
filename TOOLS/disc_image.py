"""Read ISO9660 and mixed-mode BIN/CUE without mounting or executing a disc."""
from dataclasses import dataclass
import io
import re
from pathlib import Path
import wave

SECTOR_AUDIO = 2352
MAX_BYTES = 8 * 1024**3
MAX_FILES = 50000


def safe_path(root, name):
    parts = name.replace("\\", "/").split("/")
    if not parts or any(not p or p in (".", "..") or ":" in p or "\0" in p
                        or p.endswith((" ", ".")) for p in parts):
        raise ValueError(f"Chemin non sûr dans la source : {name!r}")
    for p in parts:
        if p.split('.')[0].upper() in {"CON", "PRN", "AUX", "NUL", *(f"COM{i}" for i in range(1, 10)), *(f"LPT{i}" for i in range(1, 10))}:
            raise ValueError(f"Nom réservé : {name!r}")
    target = Path(root).joinpath(*parts)
    if not target.resolve().is_relative_to(Path(root).resolve()):
        raise ValueError(f"Chemin hors du dossier source : {name!r}")
    return target


def sectors(msf):
    match = re.fullmatch(r"(\d+):(\d{2}):(\d{2})", msf)
    if not match:
        raise ValueError(f"Position CUE incorrecte : {msf}")
    m, s, f = map(int, match.groups())
    if s >= 60 or f >= 75:
        raise ValueError(f"Position CUE incorrecte : {msf}")
    return (m * 60 + s) * 75 + f


@dataclass
class Track:
    number: int
    mode: str
    file: Path
    start: int = -1
    index0: int | None = None
    end: int = -1
    sector_size: int = 2352


def read_cue(path):
    path = Path(path)
    tracks = []
    current_file = None
    for raw in path.read_text(encoding="utf-8-sig").splitlines():
        line = raw.strip()
        if not line:
            continue
        word = line.split()[0].upper()
        if word == "FILE":
            match = re.fullmatch(r'FILE\s+(?:"([^"]+)"|(\S+))\s+BINARY', line, re.I)
            if not match:
                raise ValueError("CUE : seuls les fichiers FILE ... BINARY sont pris en charge.")
            current_file = safe_path(path.parent, match[1] or match[2])
            if not current_file.is_file() or current_file.is_symlink():
                raise ValueError(f"Fichier BIN manquant ou lien symbolique : {current_file.name}")
        elif word == "TRACK":
            match = re.fullmatch(r"TRACK\s+(\d+)\s+(AUDIO|MODE1/2352|MODE1/2048|MODE2/2352)", line, re.I)
            if not match or current_file is None:
                raise ValueError(f"Piste CUE non prise en charge : {line}")
            number, mode = int(match[1]), match[2].upper()
            if not 1 <= number <= 99 or (tracks and number <= tracks[-1].number):
                raise ValueError("Numéros de pistes CUE non croissants ou hors plage.")
            tracks.append(Track(number, mode, current_file, sector_size=2048 if mode == "MODE1/2048" else 2352))
        elif word == "INDEX":
            fields = line.split()
            if len(fields) != 3 or not tracks:
                raise ValueError(f"Index CUE incorrect : {line}")
            index, value = int(fields[1]), sectors(fields[2])
            if index == 1:
                if tracks[-1].start >= 0:
                    raise ValueError("INDEX 01 dupliqué.")
                tracks[-1].start = value
            elif index == 0:
                tracks[-1].index0 = value
        elif word in ("PREGAP", "POSTGAP"):
            # Synthetic gaps are not present in the BIN and do not move its offsets.
            sectors(line.split()[1])
        elif word not in ("REM", "TITLE", "PERFORMER", "SONGWRITER", "CATALOG", "ISRC", "FLAGS", "CDTEXTFILE"):
            raise ValueError(f"Instruction CUE inconnue : {word}")
    if not tracks:
        raise ValueError("CUE sans piste.")
    for i, track in enumerate(tracks):
        siblings = [t for t in tracks if t.file == track.file]
        if len({t.sector_size for t in siblings}) != 1:
            raise ValueError("Tailles de secteurs mixtes dans un même BIN non prises en charge.")
        size = track.file.stat().st_size
        if size % track.sector_size:
            raise ValueError(f"BIN tronqué : {track.file.name}")
        next_track = tracks[i + 1] if i + 1 < len(tracks) else None
        track.end = ((next_track.index0 if next_track.index0 is not None else next_track.start)
                     if next_track and next_track.file == track.file else size // track.sector_size)
        if not 0 <= track.start < track.end <= size // track.sector_size:
            raise ValueError(f"Bornes incorrectes pour la piste {track.number:02}.")
        if track.index0 is not None and not 0 <= track.index0 <= track.start:
            raise ValueError("INDEX 00 doit précéder INDEX 01.")
    return tracks


class SectorReader(io.RawIOBase):
    """Seekable 2048-byte data view over a track, with bounded raw sector reads."""
    def __init__(self, track):
        self.track = track
        self.fp = track.file.open("rb")
        self.position = 0
        self.length = (track.end - track.start) * 2048
        self.header = {"MODE1/2048": 0, "MODE1/2352": 16, "MODE2/2352": 24}[track.mode]

    def readable(self): return True
    def seekable(self): return True
    def tell(self): return self.position

    def seek(self, offset, whence=0):
        value = offset + (0 if whence == 0 else self.position if whence == 1 else self.length)
        if value < 0 or whence not in (0, 1, 2):
            raise ValueError("Position de lecture invalide.")
        self.position = value
        return value

    def readinto(self, buffer):
        count = min(len(buffer), max(0, self.length - self.position))
        copied = 0
        while copied < count:
            sector, inside = divmod(self.position, 2048)
            take = min(2048 - inside, count - copied)
            self.fp.seek((self.track.start + sector) * self.track.sector_size)
            raw = self.fp.read(self.track.sector_size)
            if len(raw) != self.track.sector_size:
                raise ValueError("Secteur tronqué.")
            if self.header and (raw[:12] != b"\x00" + b"\xff" * 10 + b"\x00" or raw[15] != (2 if self.header == 24 else 1)):
                raise ValueError("Secteur de données invalide (synchronisation/mode).")
            if self.header == 24 and raw[18] & 0x20:
                raise ValueError("Secteur MODE2 Form 2 non pris en charge pour ISO9660.")
            buffer[copied:copied + take] = raw[self.header + inside:self.header + inside + take]
            copied += take
            self.position += take
        return copied

    def close(self):
        self.fp.close()
        super().close()


def extract_iso(stream, destination):
    import pycdlib
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    iso = pycdlib.PyCdlib()
    total = count = 0
    seen = set()
    try:
        iso.open_fp(stream)
        volume = iso.pvd.volume_identifier.decode("ascii", errors="replace").strip()
        for directory, _, filenames in iso.walk(iso_path="/"):
            for name in filenames:
                iso_name = directory.rstrip("/") + "/" + name
                relative = "/".join(p.split(";")[0].rstrip(".") for p in iso_name.lstrip("/").split("/")).upper()
                target = safe_path(destination, relative)
                if relative.casefold() in seen:
                    raise ValueError(f"Deux noms ISO convergent vers {relative}.")
                seen.add(relative.casefold())
                record = iso.get_record(iso_path=iso_name)
                total += record.data_length
                count += 1
                if total > MAX_BYTES or count > MAX_FILES:
                    raise ValueError("Image trop volumineuse pour cet importeur (8 Gio / 50 000 fichiers).")
                target.parent.mkdir(parents=True, exist_ok=True)
                with target.open("xb") as out:
                    iso.get_file_from_iso_fp(out, iso_path=iso_name)
                if target.stat().st_size != record.data_length:
                    raise ValueError(f"Extraction incomplète : {relative}")
        return {"volume": volume, "files": count, "bytes": total}
    finally:
        iso.close()


def rip_track(track, target):
    target = Path(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    remaining = (track.end - track.start) * SECTOR_AUDIO
    with track.file.open("rb") as src, wave.open(str(target), "wb") as out:
        src.seek(track.start * SECTOR_AUDIO)
        out.setparams((2, 2, 44100, 0, "NONE", "not compressed"))
        while remaining:
            chunk = src.read(min(1024 * SECTOR_AUDIO, remaining))
            if not chunk:
                raise ValueError(f"Piste {track.number:02} tronquée.")
            out.writeframesraw(chunk)
            remaining -= len(chunk)
    return {"number": track.number, "file": target.name,
            "sectors": track.end - track.start, "seconds": (track.end - track.start) / 75}


def extract_cue(path, destination, log=print):
    tracks = read_cue(path)
    records = []
    for track in tracks:
        log(f"  Piste {track.number:02} : {track.mode}")
        if track.mode == "AUDIO":
            record = rip_track(track, Path(destination) / "music" / f"{track.number:02}.wav")
        else:
            with SectorReader(track) as raw, io.BufferedReader(raw) as stream:
                record = extract_iso(stream, Path(destination) / f"data_{track.number:02}")
        records.append({"number": track.number, "mode": track.mode, **record})
    return records
