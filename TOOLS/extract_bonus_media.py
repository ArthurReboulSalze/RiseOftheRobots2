"""Import optional original WAV announcements and FLC movies from owned media.

No recording or movie is bundled. A normal game import never requires these
extras. Movie conversion uses a locally installed FFmpeg; no downloads occur.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import wave

NAMES = dict(zip("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123", (
    "CYBORG", "LOADER", "PRIME8", "CRUSHER", "WAR", "ROOK", "V1HYPER", "DEADLIFT",
    "SUIKWAN", "DETAIN", "CHROMAX", "STEPPEN", "NECROBRG", "LOCKJAW", "GRILLER",
    "VANDAL", "SALVO", "INSANE", "SUPERVIS", "MAYHEM", "ASSAULT", "ANIL8", "NADEN",
    "RACK", "SANE", "VITRIOL", "SURPRESS", "ARDONE", "SHEEPMAN", "BUNNY")))
EVENTS = {"victory": "YOUWIN", "fight": "FIGHT", "game_over": "GAMEOVER"}


def collect_media(roots, destination):
    destination = Path(destination)
    found = {}
    for root in roots:
        for source in Path(root).rglob("*"):
            if not source.is_file() or source.suffix.upper() not in (".WAV", ".FLC"):
                continue
            if source.is_symlink() or source.is_junction():
                raise ValueError("Linked media source rejected")
            name = source.name.upper()
            if source.suffix.upper() == ".WAV" and source.parent.name.upper() not in ("WAVS", "VOICES", "SPEECH"):
                if source.stem.upper() not in set(NAMES.values()) | set(EVENTS.values()):
                    continue
            folder = "WAVS" if source.suffix.upper() == ".WAV" else "FLICS"
            signature = hashlib.sha256(source.read_bytes()).hexdigest()
            key = (folder, name)
            if key in found:
                if found[key]['sha256'] != signature:
                    raise ValueError(f"Conflicting original media: {name}")
                continue
            target = destination / folder / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            found[key] = dict(name=name, kind=folder, sha256=signature, bytes=source.stat().st_size)
    return list(found.values())


def export_flc(job):
    source, root, ffmpeg = job
    header = source.read_bytes()[:128]
    if len(header) < 128 or struct.unpack_from('<H', header, 4)[0] not in (0xaf11, 0xaf12):
        raise ValueError(f"Invalid FLI/FLC header: {source.name}")
    count, width, height = struct.unpack_from('<3H', header, 6)
    speed = struct.unpack_from('<I', header, 16)[0]
    if not 0 < count <= 10000 or not 0 < width <= 4096 or not 0 < height <= 4096:
        raise ValueError(f"Invalid FLC dimensions/count: {source.name}")
    milliseconds = speed * 1000 / 70 if header[4:6] == b'\x11\xaf' else speed
    name = 'BONUS' + re.sub('[^A-Z0-9]', '', source.stem.upper())
    output = root / name
    output.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([ffmpeg, '-nostdin', '-v', 'error', '-y', '-i', str(source),
        '-map', '0:v:0', '-frames:v', str(count), '-fps_mode', 'passthrough',
        '-threads', '1', '-compression_level', '1', '-start_number', '0',
        str(output / 'frame_%05d.png')], capture_output=True, text=True)
    frames = sorted(output.glob('frame_*.png'))
    if result.returncode or len(frames) != count:
        raise ValueError(f"FLC conversion failed for {source.name}: {result.stderr[:500]} ({len(frames)}/{count} frames)")
    manifest = dict(source=source.name, source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                    size=[width,height], frame_count=count, playable_frame_count=count,
                    kind='bonus', robot_slot='', timing_hz=1000,
                    frame_ticks=[max(1,milliseconds)]*count, frames=[p.name for p in frames])
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print(f"{source.name}: {count} frames, {milliseconds:g} ms/frame",flush=True)
    return {k:manifest[k] for k in ('source_sha256','size','frame_count','playable_frame_count','kind','robot_slot')} | dict(name=name,title=source.stem,placeholder=False)


def export_media(media, extracted, ffmpeg=None):
    media, extracted = Path(media), Path(extracted)
    voices = {}
    voice_dir = extracted/'audio/voices'
    for source in sorted((media/'WAVS').glob('*.WAV')):
        with wave.open(str(source)) as sound:
            if not sound.getnframes() or sound.getcomptype() != 'NONE':
                raise ValueError(f"Invalid voice WAV: {source.name}")
            record = dict(file=source.name,rate=sound.getframerate(),bits=8*sound.getsampwidth(),
                          channels=sound.getnchannels(),seconds=sound.getnframes()/sound.getframerate(),
                          source_sha256=hashlib.sha256(source.read_bytes()).hexdigest())
        voice_dir.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(source,voice_dir/source.name)
        voices[source.stem]=record
    robot_voices = {}
    for slot, name in NAMES.items():
        if name not in voices:
            continue
        entry = dict(name=voices[name]['file'])
        for win in (name+'WIN',name+'WINS'):
            if win in voices:
                entry['victory']=voices[win]['file']
                break
        robot_voices[slot]=entry
    if voices:
        report=dict(robots=robot_voices,events={k:voices[v]['file'] for k,v in EVENTS.items() if v in voices},clips=voices)
        (voice_dir/'manifest.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    movies = list((media/'FLICS').glob('*.FLC'))
    errors, converted = [], []
    if movies:
        ffmpeg = ffmpeg or shutil.which('ffmpeg')
        if not ffmpeg:
            errors.append('FLC files preserved. Install FFmpeg locally to convert optional bonus movies.')
        else:
            with ThreadPoolExecutor(max_workers=2) as pool:
                futures=[pool.submit(export_flc,(p,extracted/'video',ffmpeg)) for p in sorted(movies)]
                for future in futures:
                    try:
                        converted.append(future.result())
                    except ValueError as error:
                        errors.append(str(error))
    catalog_path=extracted/'video/catalog.json'
    if converted:
        catalog=json.loads(catalog_path.read_text(encoding='utf-8')) if catalog_path.exists() else dict(movies=[])
        replacements={movie['name'] for movie in converted}
        catalog['movies']=[movie for movie in catalog['movies'] if movie['name'] not in replacements]+converted
        catalog_path.parent.mkdir(parents=True,exist_ok=True)
        catalog_path.write_text(json.dumps(catalog,indent=2)+'\n',encoding='utf-8')
    return dict(voice_clips=len(voices),robot_names=len(robot_voices),bonus_movies=len(movies),
                converted_bonus_movies=len(converted),warnings=errors)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,nargs='+',required=True,help='Folders containing original WAV/FLC media')
    parser.add_argument('--media-output',type=Path,required=True)
    parser.add_argument('--extracted',type=Path,required=True)
    parser.add_argument('--ffmpeg')
    args=parser.parse_args()
    collect_media(args.source,args.media_output)
    report=export_media(args.media_output,args.extracted,args.ffmpeg)
    print(json.dumps(report,indent=2))
    if report['warnings']:
        raise SystemExit(1)


if __name__=='__main__':
    main()
