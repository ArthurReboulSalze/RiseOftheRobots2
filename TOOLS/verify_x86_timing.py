"""Verify combat timing by running instructions from a user-supplied DOS EXR.

Optional dependency: pip install --target ANALYSIS/verification_deps unicorn
Only the two mapped executable hashes are accepted. Original code stays local.
Audio, input, rendering and combat callbacks are counted/skipped; the native
clock arithmetic and pending-tick loop run unchanged, with no DOS game launch.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import sys

from le_codec import flatten
from project_paths import ROOT, SOURCE, ANALYSIS

sys.path.insert(0, str(ROOT / 'ANALYSIS/verification_deps'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP

BUILDS = {
    '213ba86c2292cb3203f87826c030fbdb9d4c77c3a8c27d69fce230a53dc4241b':
        dict(edition='older DOS', init=0x11101, irq=0x1444d, advance=0x1454e,
             rate=0x663a2, divisor=0x663a0, accumulator=0x6639a, audio_accumulator=0x6639e,
             pending=0x6636a, consumed=0x66372, phase=0x66366, paused=0x6635e,
             winner=0x66390, combat=0x2163a),
    'ea6730a421bcd6f7a8aefcfdec59c2b6ee1e91430305b72cb96c0a0fa5b5d135':
        dict(edition="Director's Cut Disc 1", init=0x11101, irq=0x144fe, advance=0x145ff,
             rate=0x663b0, divisor=0x663ae, accumulator=0x663a4, audio_accumulator=0x663a8,
             pending=0x66384, consumed=0x6635a, phase=0x66378, paused=0x6636e,
             winner=0x66386, combat=0x2188a),
}
STOP, STACK = 0x1000, 0xf00000


def verify(source):
    raw = (source / 'RISE2.EXR').read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest not in BUILDS:
        raise ValueError('Unmapped EXR: analyse its timing addresses first')
    build = BUILDS[digest]
    image, metadata = flatten(raw)

    def machine(start, end):
        mu = Uc(UC_ARCH_X86, UC_MODE_32)
        mu.mem_map(0, 0x1000000)
        mu.mem_write(metadata['base'], image)
        calls = Counter()

        def skip_callback(mu, address, size, unused):
            if start <= address < end and size == 5 and mu.mem_read(address, 1) == b'\xe8':
                target = address + 5 + struct.unpack('<i', mu.mem_read(address + 1, 4))[0]
                calls[target] += 1
                mu.reg_write(UC_X86_REG_EIP, address + size)

        mu.hook_add(UC_HOOK_CODE, skip_callback)
        return mu, calls

    def word(mu, address):
        return struct.unpack('<h', mu.mem_read(address, 2))[0]

    def put(mu, address, value):
        mu.mem_write(address, struct.pack('<h', value))

    def run(mu, entry):
        mu.mem_write(STACK, struct.pack('<I', STOP))
        mu.reg_write(UC_X86_REG_ESP, STACK)
        mu.reg_write(UC_X86_REG_EAX, 0)
        mu.emu_start(entry, STOP, count=20000)
        if mu.reg_read(UC_X86_REG_EIP) != STOP:
            raise ValueError('Native timing function did not return')

    mu, calls = machine(build['irq'], build['advance'])
    # Execute the two original initialization stores, not a substituted rate.
    mu.emu_start(build['init'], build['init'] + 18, count=2)
    rate, divisor = word(mu, build['rate']), word(mu, build['divisor'])
    if (rate, divisor) != (25, 100):
        raise ValueError('Unexpected native clock initialization')
    for address in (build['pending'], build['accumulator'], build['audio_accumulator'], 0x50996, 0x50684):
        put(mu, address, 0)
    # Native comparisons are strict >; discard the cold-start phase offset.
    for _ in range(4):
        run(mu, build['irq'])
    rates = []
    for _ in range(3):
        before = word(mu, build['pending'])
        for _ in range(100):
            run(mu, build['irq'])
        rates.append(word(mu, build['pending']) - before)
    if rates != [25, 25, 25]:
        raise ValueError(f'Unexpected native tick rate: {rates}')

    cases = []
    for phase, paused, pending in ((1, 0, 0), (1, 0, 1), (1, 0, 7), (3, 0, 7), (1, 1, 7)):
        mu, calls = machine(build['advance'], build['advance'] + 0x180)
        for address, value in ((build['pending'], pending), (build['phase'], phase),
                               (build['paused'], paused), (build['winner'], 1)):
            put(mu, address, value)
        run(mu, build['advance'])
        updates = calls[build['combat']]
        if (word(mu, build['pending']), word(mu, build['consumed']), updates) != (0, pending, 0 if paused else pending):
            raise ValueError('Native loop lost pending ticks or advanced while paused')
        cases.append(dict(phase=phase, paused=bool(paused), pending=pending, combat_updates=updates))
    return dict(edition=build['edition'], executable_sha256=digest,
                clock_rate=rate, irq_divisor=divisor, ticks_per_100_interrupts=rates, cases=cases)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=SOURCE)
    parser.add_argument('--output', type=Path, default=ANALYSIS / 'timing_x86.json')
    args = parser.parse_args()
    report = verify(args.source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))
