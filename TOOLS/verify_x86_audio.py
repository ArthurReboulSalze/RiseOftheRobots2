"""Verify audio settings by emulating the supplied DOS executable's instructions.

Optional dependency: python -m pip install --target ANALYSIS/verification_deps unicorn
The two analysed EXR builds are identified by SHA-256. No original code or PCM is
embedded here. Execution stops before DOS/driver entry points; no game is launched.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

from le_codec import flatten
from project_paths import ROOT, SOURCE, ANALYSIS

sys.path.insert(0, str(ROOT / "ANALYSIS/verification_deps"))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_ESP

BUILDS = {
    "213ba86c2292cb3203f87826c030fbdb9d4c77c3a8c27d69fce230a53dc4241b":
        dict(edition="older DOS", init=0x3a3e8, init_stop=0x3a4c1,
             quality=0x62e80, driver=0x62c78, enabled=0x703d8,
             play=0x3a792, start=0x417c9, sample=0x62d84,
             banks=0x705bc, volume=0x70450),
    "ea6730a421bcd6f7a8aefcfdec59c2b6ee1e91430305b72cb96c0a0fa5b5d135":
        dict(edition="Director's Cut Disc 1", init=0x3a8c3, init_stop=0x3a99c,
             quality=0x62e84, driver=0x62c7c, enabled=0x70428,
             play=0x3acb0, start=0x41d39, sample=0x62d88,
             banks=0x705c8, volume=0x7044e),
}
STOP, STACK, BANK = 0x1000, 0xf00000, 0x800000


def verify(source):
    raw = (source / "RISE2.EXR").read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest not in BUILDS:
        raise ValueError("Unmapped EXR build: analyse its addresses in Ghidra first.")
    build = BUILDS[digest]
    image, metadata = flatten(raw)

    def machine():
        mu = Uc(UC_ARCH_X86, UC_MODE_32)
        mu.mem_map(0, 0x1000000)
        mu.mem_write(metadata["base"], image)
        mu.mem_write(STACK, struct.pack("<I", STOP))
        mu.reg_write(UC_X86_REG_ESP, STACK)
        mu.mem_write(build["enabled"], struct.pack("<I", 0))
        return mu

    def run(mu, entry, breakpoint, collect):
        result = []
        def hook(mu, address, size, context):
            result.append(collect(mu))
            mu.emu_stop()
        mu.hook_add(UC_HOOK_CODE, hook, begin=breakpoint, end=breakpoint)
        mu.emu_start(entry, STOP, count=10000)
        if len(result) != 1:
            raise ValueError("Audio code did not reach the expected driver boundary.")
        return result[0]

    def u32(mu, address):
        return struct.unpack("<I", mu.mem_read(address, 4))[0]

    qualities = []
    for quality in range(5):
        mu = machine()
        mu.mem_write(build["quality"], struct.pack("<H", quality))
        settings = run(mu, build["init"], build["init_stop"],
                       lambda m: dict(rate=u32(m, build["driver"] + 4),
                                      channels=u32(m, build["driver"] + 8),
                                      bits=u32(m, build["driver"] + 12)))
        qualities.append(dict(index=quality, **settings))
    if [q["rate"] for q in qualities] != [5000, 6500, 8000, 9500, 11025]:
        raise ValueError("Unexpected sound-quality table.")

    bank = (source / "R0.MRW").read_bytes()
    offset, length = struct.unpack_from("<II", bank, 2 + 15 * 8)
    samples = []
    for multiplier, rate in [(0x10000, 11025), (0x20000, 22050)]:
        mu = machine()
        mu.mem_write(BANK, bank)
        mu.mem_write(build["banks"] + 4, struct.pack("<I", BANK))
        mu.mem_write(build["volume"], struct.pack("<H", 48))
        for reg, value in [(UC_X86_REG_EAX, 15), (UC_X86_REG_EDX, 0x2000),
                           (UC_X86_REG_EBX, 1), (UC_X86_REG_ECX, multiplier)]:
            mu.reg_write(reg, value)
        descriptor = run(mu, build["play"], build["start"],
                         lambda m: dict(pointer=u32(m, build["sample"]),
                                        length=u32(m, build["sample"] + 12),
                                        volume=u32(m, build["sample"] + 44),
                                        rate=u32(m, build["sample"] + 52)))
        if descriptor != dict(pointer=BANK + offset, length=length,
                              volume=0x20002000, rate=rate):
            raise ValueError("Unexpected SOS sample descriptor.")
        samples.append(dict(multiplier=multiplier, **descriptor))
    return dict(edition=build["edition"], executable_sha256=digest,
                qualities=qualities, sample_15=samples)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=SOURCE)
    parser.add_argument("--output", type=Path, default=ANALYSIS / "audio_x86.json")
    args = parser.parse_args()
    report = verify(args.source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
