"""Import private Rise 2 sources and rebuild assets locally; never run a game executable."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import wave
import zipfile

from disc_image import MAX_BYTES, MAX_FILES, extract_cue, extract_iso, safe_path
from project_paths import ROOT

SLOTS = "01ABCDEFGHIJKLMNOPQRSTUVWXYZ"
MUSIC_EXTENSIONS = {".wav", ".mp3", ".flac", ".ogg", ".m4a"}


def digest(path):
    with Path(path).open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()


def write_json(path, data):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def files_under(root):
    for parent, dirs, files in os.walk(root, followlinks=False):
        for name in dirs + files:
            p = Path(parent) / name
            if p.is_symlink() or p.is_junction():
                raise ValueError(f"Symbolic link or junction rejected in source : {p}")
        dirs.sort()
        for name in sorted(files):
            yield Path(parent) / name


def unzip(source, destination):
    destination = Path(destination)
    seen = set()
    total = 0
    with zipfile.ZipFile(source) as archive:
        entries = archive.infolist()
        if len(entries) > MAX_FILES:
            raise ValueError("ZIP: too many files.")
        for entry in entries:
            name = entry.filename.rstrip("/")
            target = safe_path(destination, name)
            if name.casefold() in seen:
                raise ValueError(f"ZIP: duplicate name : {name}")
            seen.add(name.casefold())
            if stat.S_ISLNK(entry.external_attr >> 16) or entry.flag_bits & 1:
                raise ValueError("ZIP: symbolic links and encrypted archives are not accepted.")
            total += entry.file_size
            if total > MAX_BYTES:
                raise ValueError("Uncompressed ZIP exceeds 8 GiB.")
        for entry in entries:
            target = safe_path(destination, entry.filename.rstrip("/"))
            if entry.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(entry) as src, target.open("xb") as out:
                shutil.copyfileobj(src, out, 1024 * 1024)
    return destination


def game_roots(root):
    by_parent = {}
    for p in files_under(root):
        by_parent.setdefault(p.parent, set()).add(p.name.upper())
    return [p for p, names in by_parent.items() if {"RBT0.MVS", "RBT0.ANR"} <= names]


def optical_drive(path):
    if os.name != "nt":
        return False
    import ctypes
    return ctypes.windll.kernel32.GetDriveTypeW(str(Path(path).anchor)) == 5


def import_source(source, dest, log):
    source = Path(source).resolve()
    if not source.exists():
        raise ValueError(f"Source not found : {source}")
    dest.mkdir(parents=True, exist_ok=True)
    record = {"name": source.name or str(source), "kind": "folder"}
    if source.is_file():
        suffix = source.suffix.lower()
        if suffix == ".bin":
            cues = [p for p in source.parent.iterdir() if p.suffix.lower() == ".cue" and p.stem.casefold() == source.stem.casefold()]
            if len(cues) != 1:
                raise ValueError("A BIN must have its CUE file to identify track bounds and numbers.")
            source, suffix = cues[0], ".cue"
        record.update(kind=suffix[1:], name=source.name, sha256=digest(source))
        if suffix == ".cue":
            record["tracks"] = extract_cue(source, dest, log)
            return game_roots(dest), [dest / "music"] if (dest / "music").exists() else [], record
        if suffix == ".iso":
            with source.open("rb") as stream:
                record["iso"] = extract_iso(stream, dest / "data")
            record["notice"] = "Data-only ISO: no CD audio tracks included."
            return game_roots(dest / "data"), [], record
        if suffix != ".zip":
            raise ValueError("Accepted sources: folder, ZIP, ISO, or CUE with BIN.")
        source = unzip(source, dest / "archive")
    roots = game_roots(source)
    if roots:
        music = []
        if optical_drive(source):
            from physical_cd import rip_windows_cd
            record["kind"] = "physical_cd"
            try:
                record["tracks"] = rip_windows_cd(source.anchor, dest / "music", log)
            except (OSError, ValueError) as exc:
                # Some virtual data-only drives do not expose a CDDA TOC. Preserve
                # the data import; an interrupted audio track remains a hard error.
                if any((dest / "music").glob("*.wav")):
                    raise
                record["tracks"] = []
                record["audio_error"] = str(exc)
                log(f"  CD tracks unreadable on this drive : {exc}")
            if record["tracks"]:
                music.append(dest / "music")
        return roots, music, record
    images = [p for p in files_under(source) if p.suffix.lower() in (".cue", ".iso")]
    if not images:
        # A supplemental disc may contain only movies/bonus material.
        if record["kind"] != "folder":
            return [], [], record
        raise ValueError("No complete game folder or ISO/CUE image found. The small installation folder alone is not enough.")
    roots, music, children = [], [], []
    for i, path in enumerate(images, 1):
        child_roots, child_music, child = import_source(path, dest / f"disc_{i:02}", log)
        roots.extend(child_roots)
        music.extend(child_music)
        children.append(child)
    record["discs"] = children
    return roots, music, record


def required_files():
    names = {"MAINSCR.GGF", "VS.GGF", "LLOGO.ANI", "VSFACE.ANL", "VSFACE.ANR", "VSFACE.PAL"}
    for slot in SLOTS:
        names.update({f"RBT{slot}.{ext}" for ext in ("ANL", "ANR", "MVS")})
        names.update((f"R{slot}.CL2", f"R{slot}.PAL", f"AG{slot}.GGF"))
    return names


def copy_game(roots, output):
    unique = []
    for root in roots:
        names = {p.name.upper(): p for p in root.iterdir() if p.is_file()}
        signature = tuple((name, digest(names[name])) for name in sorted(names))
        if not any(signature == previous for _, previous in unique):
            unique.append((root, signature))
    if not unique:
        raise ValueError("No RBT0 bank found in the sources. The game disc is required; the bonus disc alone is insufficient.")
    if len(unique) != 1:
        raise ValueError("Different game editions detected. Import each edition into a separate profile.")
    source = unique[0][0]
    output.mkdir()
    records = []
    for path in sorted(source.iterdir()):
        if not path.is_file():
            continue
        if path.is_symlink():
            raise ValueError(f"Symbolic link rejected : {path.name}")
        dest = safe_path(output, path.name.upper())
        if dest.exists():
            raise ValueError(f"Filename collision : {path.name}")
        shutil.copyfile(path, dest)
        records.append({"name": dest.name, "size": dest.stat().st_size, "sha256": digest(dest)})
    return source, records


def track_number(path):
    match = re.fullmatch(r"(?:(?:piste|track|audio)[ _-]*)?(\d{1,2})(?:[ ._-].*)?", path.stem, re.I)
    if not match or not 2 <= int(match[1]) <= 99:
        raise ValueError(f"Ambiguous track number : {path.name}. Use 02, 03, ... (track 01 contains data).")
    return int(match[1])


def audio_info(path):
    with path.open("rb") as f:
        header = f.read(16)
    ext = path.suffix.lower()
    valid = ((ext == ".wav" and header[:4] == b"RIFF" and header[8:12] == b"WAVE") or
             (ext == ".mp3" and (header[:3] == b"ID3" or (len(header) >= 2 and header[0] == 255 and header[1] & 224 == 224))) or
             (ext == ".flac" and header[:4] == b"fLaC") or
             (ext == ".ogg" and header[:4] == b"OggS") or
             (ext == ".m4a" and header[4:8] == b"ftyp"))
    if not valid:
        raise ValueError(f"Invalid audio header : {path.name}")
    if ext == ".wav":
        with wave.open(str(path)) as f:
            if f.getnframes() == 0:
                raise ValueError(f"Empty track : {path.name}")
            return {"seconds": f.getnframes() / f.getframerate(), "rate": f.getframerate(), "channels": f.getnchannels()}
    return {"validation": "signature validated; file retained without re-encoding"}


def import_music(sources, destination):
    candidates = {}
    for source in sources:
        for path in files_under(source):
            if path.suffix.lower() not in MUSIC_EXTENSIONS:
                continue
            number = track_number(path)
            if number in candidates:
                raise ValueError(f"Two files use track number {number:02}.")
            candidates[number] = path
    destination.mkdir(parents=True, exist_ok=True)
    records = []
    for number, path in sorted(candidates.items()):
        info = audio_info(path)
        target = destination / f"{number:02}{path.suffix.lower()}"
        shutil.copyfile(path, target)
        records.append({"number": number, "file": target.name, "sha256": digest(target), **info})
    write_json(destination / "manifest.json", {"tracks": records})
    return records


def choose_music(mode, names, tracks):
    digital = all(f"MG{x}.{ext}" in names for x in "ABCDEF" for ext in ("MRS", "MRW"))
    effects = any(n.startswith("BG") and n.endswith(".MRS") and n[:-4] + ".MRW" in names for n in names)
    available = {"cd": bool(tracks), "digital": digital, "effects": effects, "off": True}
    selected = next((v for v in ("cd", "digital", "effects", "off") if available[v])) if mode == "auto" else mode
    if not available[selected]:
        raise ValueError(f"Requested audio mode {selected} has no available data.")
    return {"requested": mode, "selected": selected, "available": available,
            "digital_banks": [f"MG{x}" for x in "ABCDEF" if f"MG{x}.MRS" in names and f"MG{x}.MRW" in names],
            "runtime_playback": {"cd_tracks": "implemented", "digital_mrs": "not_implemented"},
            "note": "Import configuration. The port can play imported CD tracks; digital MRS sequencing is not yet implemented."}


def convert(game, extracted, log=print):
    jobs = [
        ("extract_ggf.py", "--source", game, "--output", extracted / "ggf"),
        ("extract_anr.py", "--source", game, "--output", extracted / "sprites"),
        ("extract_mvs.py", "--source", game, "--output", extracted / "data/mvs"),
        ("extract_cl2.py", "--source", game, "--output", extracted / "data/cl2"),
        ("extract_mrw_audio.py", "--source", game, "--output", extracted / "audio/mrw"),
        ("extract_ani.py", "--source-dir", game, "--output-dir", extracted / "video"),
        ("build_ui_font.py", "--output", extracted / "ui/font.png"),
        ("build_image_gallery.py", "--extracted", extracted),
    ]
    for job in jobs:
        log(f"Converting: {job[0]}")
        subprocess.run([sys.executable, "-u", str(ROOT / "TOOLS" / job[0]), *map(str, job[1:])], check=True)
    # Validate the contracts actually consumed by the C++ runtime.
    for slot in SLOTS + "23":
        if not (extracted / f"sprites/RBT{slot}/manifest.json").exists():
            if slot in SLOTS:
                raise ValueError(f"Required atlas missing : RBT{slot}")
            continue
        atlas = json.loads((extracted / f"sprites/RBT{slot}/manifest.json").read_text())
        mvs = json.loads((extracted / f"data/mvs/RBT{slot}.json").read_text())
        cl2 = json.loads((extracted / f"data/cl2/R{slot}.json").read_text())
        if atlas["frame_count"] != cl2["count"] + 1 or len(mvs["moves"]) != 96:
            raise ValueError(f"Incompatible atlas/MVS/CL2 data for robot {slot}.")
        for move in mvs["moves"]:
            for sequence in move["sequences"]:
                if not sequence or not sequence[-1].get("end") or any(e.get("truncated") for e in sequence):
                    raise ValueError(f"Truncated MVS sequence : RBT{slot}")
                if any(not 0 <= e["image"] < cl2["count"] for e in sequence if "image" in e):
                    raise ValueError(f"MVS image index out of range : RBT{slot}")


def run_import(sources, output, music=(), music_mode="auto", convert_assets=True, log=print):
    output = Path(output).resolve()
    if output.exists():
        raise ValueError(f"Profile already exists : {output}. Choose a new name; existing imports are never overwritten.")
    for source in sources:
        p = Path(source).resolve()
        if p.is_dir() and output.is_relative_to(p):
            raise ValueError("The output profile must be outside the source folder.")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".rise2-import-", dir=output.parent) as temp:
        staging = Path(temp) / "profile"
        staging.mkdir()
        roots, music_roots, source_records = [], [], []
        physical_music = {}
        for i, source in enumerate(sources, 1):
            log(f"Import : {Path(source).name or source}")
            rs, ms, record = import_source(source, staging / "sources" / f"{i:02}", log)
            roots.extend(rs)
            music_roots.extend(ms)
            source_records.append(record)
            if record["kind"] == "physical_cd":
                for root in rs:
                    physical_music[root] = ms
        game_source, game_records = copy_game(roots, staging / "game")
        names = {entry["name"] for entry in game_records}
        missing = sorted(required_files() - names)
        if missing:
            raise ValueError(f"Required port data missing ({len(missing)}) : {', '.join(missing[:12])}")
        selected_music = []
        if music:
            for i, path in enumerate(music):
                path = Path(path).resolve()
                if path.is_file() and path.suffix.lower() == ".zip":
                    path = unzip(path, staging / "sources" / f"music_{i:02}")
                if not path.is_dir():
                    raise ValueError(f"Music folder or ZIP not found : {path}")
                selected_music.append(path)
            log("Music: separately supplied folders take priority over disc-image tracks.")
        elif music_roots:
            # Select the soundtrack from the disc containing the game, not the bonus disc.
            matching = [p for p in music_roots if game_source.is_relative_to(p.parent)]
            matching = physical_music.get(game_source, matching)
            if len(matching) > 1:
                raise ValueError("Ambiguous music disc: use --music to select the tracks.")
            selected_music = matching
        tracks = import_music(selected_music, staging / "music")
        if music and not tracks:
            raise ValueError("No numbered audio tracks found in --music.")
        audio = choose_music(music_mode, names, tracks)
        warnings = []
        if any(record.get("audio_error") for record in source_records):
            warnings.append("The virtual/optical drive did not provide CD audio tracks; game data was imported.")
        if not tracks:
            warnings.append("No CD audio tracks found. MRS/MRW digital music remains available when present.")
        elif {t["number"] for t in tracks} != set(range(2, 11)):
            warnings.append("Expected CD tracks 02–10 are incomplete or different; track numbers were preserved.")
        movies = sorted(n for n in names if n.endswith(".ANI"))
        pending_movies = [n for n in movies if n not in ("LLOGO.ANI", "END.ANI", "ENL.ANI")]
        if pending_movies:
            warnings.append(f"{len(pending_movies)} additional ANI files preserved; their conversion is not yet automated.")
        else:
            warnings.append("Only the three known ANI files are present; longer cinematics may be missing.")
        if convert_assets:
            convert(staging / "game", staging / "EXTRACTED", log)
        report = {"schema": 1, "sources": source_records, "game_files": game_records,
                  "music": {**audio, "tracks": tracks}, "movies": movies,
                  "unconverted_movies": pending_movies, "assets_converted": convert_assets,
                  "robot_slots": [s for s in SLOTS + "23" if f"RBT{s}.ANL" in names],
                  "warnings": warnings}
        write_json(staging / "import-report.json", report)
        write_json(staging / "settings.json", {"schema": 1, "music": audio, "assets": "EXTRACTED", "cd_audio": "music"})
        staging.rename(output)
    for warning in warnings:
        log(f"Note: {warning}")
    log(f"Import complete : {output}")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", nargs="+", type=Path, required=True, help="Folders, ZIP, ISO, or CUE (multiple discs allowed)")
    parser.add_argument("--music", nargs="+", type=Path, default=[], help="Folders or ZIPs containing tracks 02, 03, ...")
    parser.add_argument("--music-mode", choices=("auto", "cd", "digital", "effects", "off"), default="auto")
    parser.add_argument("--output", type=Path, default=ROOT / "LOCAL/default")
    parser.add_argument("--import-only", action="store_true", help="Import and validate sources without converting images")
    args = parser.parse_args()
    try:
        run_import(args.source, args.output, args.music, args.music_mode, not args.import_only)
    except (ValueError, OSError, zipfile.BadZipFile, subprocess.CalledProcessError) as exc:
        parser.exit(1, f"Import stopped : {exc}\nThe destination profile was not published.\n")


if __name__ == "__main__":
    main()
