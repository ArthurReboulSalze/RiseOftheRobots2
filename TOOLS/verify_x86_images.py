"""Differential validation against the game's original 32-bit x86 instructions.

Optional dependency: python -m pip install --target ANALYSIS/verification_deps unicorn
Executes code from ANALYSIS/RISE2_flat_padded.bin. Only DOS file I/O is mocked
for LZW; the original dictionary/bit reader run unchanged. The ANR token loop
is stopped before video drawing and its decoded spans are compared to Python.
"""

import hashlib
import json
from pathlib import Path
import struct
import sys

from extract_ggf import ROOT, SRC, read_ggf
from sprite_codec import read_bank

sys.path.insert(0, str(ROOT / 'ANALYSIS/verification_deps'))
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP

CODE = (ROOT / 'ANALYSIS/RISE2_flat_padded.bin').read_bytes()
SOURCE, DEST, STACK = 0x800000, 0x200000, 0xf00000


def machine():
    mu = Uc(UC_ARCH_X86, UC_MODE_32)
    mu.mem_map(0, 0x1000000)
    mu.mem_write(0, CODE)
    return mu


def verify_lzw(name, relocate_dictionary=False):
    source = (SRC / name).read_bytes()
    expected = read_ggf(SRC / name)
    mu = machine()
    file_pos = 0
    width_max = 0
    for address, value in [(0x65bc4,DEST),(0x65b9c,0x300000),(STACK,0x1000)]:
        mu.mem_write(address, struct.pack('<I', value))
    mu.mem_write(0x6634a, b'\x01\x00')
    mu.reg_write(UC_X86_REG_ESP, STACK)

    def io_hook(mu, address, size, _):
        nonlocal file_pos
        if address == 0x1b304:
            mu.reg_write(UC_X86_REG_EAX, 0)
        elif address == 0x1b39e:
            count = mu.reg_read(UC_X86_REG_EDX)
            chunk = source[file_pos:file_pos+count]
            if chunk:
                mu.mem_write(mu.reg_read(UC_X86_REG_EAX), chunk)
            file_pos += len(chunk)
            mu.reg_write(UC_X86_REG_EAX, len(chunk))
        sp = mu.reg_read(UC_X86_REG_ESP)
        ret, = struct.unpack('<I', mu.mem_read(sp, 4))
        mu.reg_write(UC_X86_REG_ESP, sp + 4)
        mu.reg_write(UC_X86_REG_EIP, ret)

    for address in (0x1b304, 0x1b39e, 0x1b3e6):
        mu.hook_add(UC_HOOK_CODE, io_hook, begin=address, end=address)
    if relocate_dictionary:
        # Diagnostic for A6K only: isolate the suffix/prefix arrays. The original
        # reserves only 0x200 bytes between these arrays although it grows codes.
        def relocate(mu, address, size, _):
            mu.mem_write(0x72068,struct.pack('<II',0x400000,0x404000))
        mu.hook_add(UC_HOOK_CODE,relocate,begin=0x3c7a2,end=0x3c7a2)
    def width_hook(mu, address, size, _):
        nonlocal width_max
        width_max = max(width_max, struct.unpack('<H',mu.mem_read(0x7207a,2))[0])
    mu.hook_add(UC_HOOK_CODE, width_hook, begin=0x3caac, end=0x3caac)
    try:
        mu.emu_start(0x3c731, 0x1000, count=30000000)
    except UcError as exc:
        return {'file':name, 'pixels':len(expected.pixels), 'match':False,
                'reference_error':str(exc), 'pc':hex(mu.reg_read(UC_X86_REG_EIP)),
                'max_code_width':width_max}
    if mu.reg_read(UC_X86_REG_EIP) != 0x1000:
        raise ValueError(f'{name}: x86 execution did not terminate')
    actual = bytes(mu.mem_read(DEST, len(expected.pixels)))
    if actual != expected.pixels:
        at = next(i for i,(a,b) in enumerate(zip(actual,expected.pixels)) if a!=b)
        raise ValueError(f'{name}: x86/Python mismatch at pixel {at}')
    return {'file': name, 'pixels':len(actual), 'max_code_width':width_max,
            'dictionary_relocated':relocate_dictionary, 'match':True}


def verify_anr():
    mu = machine()
    actual = []
    def collect(mu, address, size, _):
        x = mu.reg_read(UC_X86_REG_EDX)
        y = mu.reg_read(UC_X86_REG_EAX)
        length = mu.reg_read(UC_X86_REG_ECX)
        ptr = mu.reg_read(UC_X86_REG_ESI)
        actual.append((x,y,bytes(mu.mem_read(ptr,length))))
        mu.reg_write(UC_X86_REG_ESI, ptr+length)
        mu.reg_write(UC_X86_REG_EIP, 0x1c9d6)
    mu.hook_add(UC_HOOK_CODE, collect, begin=0x1ca72, end=0x1ca72)
    report = []
    for path in sorted(SRC.glob('*.ANL')):
        raw = path.with_suffix('.ANR').read_bytes()
        mu.mem_write(SOURCE, raw)
        frames = read_bank(path)
        chosen = sorted(set((0,min(1,len(frames)-1),len(frames)//2,len(frames)-1)))
        for i in chosen:
            actual.clear()
            frame = frames[i]
            mu.mem_write(0x61c7f, bytes(8))
            mu.reg_write(UC_X86_REG_ESI,SOURCE+frame.offset)
            mu.emu_start(0x1c9d6, 0x1cb2f, count=2000000)
            if mu.reg_read(UC_X86_REG_EIP) != 0x1cb2f:
                raise ValueError(f'{path.stem}/{i}: x86 did not terminate')
            expected = [(s.x,s.y,s.pixels) for s in frame.spans]
            if actual != expected or mu.reg_read(UC_X86_REG_ESI) != SOURCE+frame.end:
                raise ValueError(f'{path.stem}/{i}: x86/Python mismatch')
        report.append({'bank':path.stem,'frames':chosen,'match':True})
    return report


if __name__ == '__main__':
    report = {'code_sha256': hashlib.sha256(CODE).hexdigest(), 'lzw':[], 'anr':[]}
    for name in ['A4A.GGF','AG0.GGF','A6K.GGF','VS.GGF']:
        result = verify_lzw(name)
        report['lzw'].append(result)
        print(result,flush=True)
    report['a6k_diagnostic'] = verify_lzw('A6K.GGF', relocate_dictionary=True)
    print(report['a6k_diagnostic'],flush=True)
    report['anr'] = verify_anr()
    report['summary'] = {'lzw_matches':sum(r['match'] for r in report['lzw']),
                         'lzw_reference_errors':[r['file'] for r in report['lzw'] if not r['match']],
                         'anr_banks':len(report['anr']),
                         'anr_frames':sum(len(b['frames']) for b in report['anr']), 'all_anr_match':True}
    (ROOT/'EXTRACTED/validation_x86.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(report['summary'])
    if report['summary']['lzw_reference_errors'] != ['A6K.GGF'] or not report['a6k_diagnostic']['match']:
        raise SystemExit('Unexpected x86 reference result; inspect validation_x86.json')
