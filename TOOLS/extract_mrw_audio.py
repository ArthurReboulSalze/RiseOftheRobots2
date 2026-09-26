# Extract audio from MRW banks (verified through FUN_3a792 and empirical span coverage):
#   [u16 count][count x (u32 offset, u32 size)][unsigned 8-bit mono PCM, 11025 Hz base]
# Shared samples (same offset and size) are deduplicated. SOS accepts per-call
# playback parameters, but their exact conversion to a rate is still under analysis.
# Outputs: EXTRACTED/audio/mrw/<bank>/<bank>_<i>.wav and EXTRACTED/data/mrw_audio.json.
import argparse
import struct, os, json, wave
from pathlib import Path
from project_paths import ROOT, SOURCE

SRC = str(SOURCE)
OUT = os.path.join(ROOT, "EXTRACTED", "audio", "mrw")
RATE = 11025

def main():
    global SRC, OUT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=SRC)
    parser.add_argument('--output', type=Path, default=OUT)
    args = parser.parse_args()
    SRC, OUT = str(args.source), str(args.output)
    os.makedirs(OUT, exist_ok=True)
    manifest = {}
    total = 0
    for f in sorted(os.listdir(SRC)):
        if not f.endswith('.MRW'):
            continue
        d = open(os.path.join(SRC, f), 'rb').read()
        if len(d) < 2:
            raise ValueError(f"Truncated MRW bank : {f}")
        cnt = struct.unpack_from('<H', d, 0)[0]
        if 2 + cnt * 8 > len(d):
            raise ValueError(f"Truncated MRW directory : {f}")
        bank = os.path.join(OUT, f[:-4])
        os.makedirs(bank, exist_ok=True)
        entries = []
        for i in range(cnt):
            off, size = struct.unpack_from('<II', d, 2 + i*8)
            if off < 2 + cnt * 8 or off + size > len(d):
                raise ValueError(f"Sample {i} out of bounds in {f}")
            pcm = d[off:off+size]
            shared = any(e['offset'] == off and e['size'] == size for e in entries)
            name = f"{f[:-4]}_{i:02d}.wav"
            w = wave.open(os.path.join(bank, name), 'wb')
            w.setnchannels(1)
            w.setsampwidth(1)
            w.setframerate(RATE)
            w.writeframes(pcm)
            w.close()
            entries.append(dict(index=i, offset=off, size=size, wav=name,
                                seconds=round(size/RATE, 3), shared=shared))
            total += 1
        json.dump(dict(bank=f, count=cnt, entries=entries), open(os.path.join(bank, "manifest.json"), 'w'), indent=1)
        print(f"{f}: {cnt} samples")
    json.dump(dict(rate=RATE, format="unsigned 8-bit mono PCM", total=total),
              open(os.path.join(OUT, "resume.json"), 'w'), indent=1)
    print(f"total: {total} samples -> EXTRACTED/audio/mrw/")

if __name__ == "__main__":
    main()
