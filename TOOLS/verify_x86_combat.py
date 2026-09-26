"""Execute original DOS command and MVS effect instructions on private files.

Optional: pip install --target ANALYSIS/verification_deps unicorn
No original instructions, samples or input scripts are embedded in this tool.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

from extract_mvs import parse_bank
from le_codec import flatten
from project_paths import SOURCE, ANALYSIS

sys.path.insert(0, str(ANALYSIS / 'verification_deps'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP

SHA = '213ba86c2292cb3203f87826c030fbdb9d4c77c3a8c27d69fce230a53dc4241b'
BANK, STS, STACK, STOP = 0x800000, 0x810000, 0xf00000, 0x1000


def verify(source, bank_name='RBTF'):
    raw = (source / 'RISE2.EXR').read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != SHA:
        raise ValueError('Unmapped executable; analyse its addresses first')
    image, metadata = flatten(raw)
    bank = bytearray((source / f'{bank_name}.MVS').read_bytes())
    parsed = parse_bank(source / f'{bank_name}.MVS')
    robot_id='ABCDEFGHIJKLMNOPQRSTUVWXYZ012345'.index(bank_name[-1])
    for move in parsed['moves']:
        for field in range(7):
            pos = move['descriptor_offset'] + field*4
            pointer = struct.unpack_from('<I',bank,pos)[0]
            struct.pack_into('<I',bank,pos,BANK+pointer)

    def machine():
        mu = Uc(UC_ARCH_X86,UC_MODE_32)
        mu.mem_map(0,0x1000000)
        mu.mem_write(metadata['base'],image)
        mu.mem_write(BANK,bytes(bank))
        mu.mem_write(STS,(source / f'{bank_name}.STS').read_bytes())
        mu.mem_write(0x685cc,struct.pack('<II',BANK+12,BANK+12))
        mu.mem_write(0x68524,bytes(bank[4:8])*2)
        mu.mem_write(0x685d4,bytes(bank[8:12])*2)
        mu.mem_write(0x703b2,struct.pack('<I',0xffff0000))
        for side in range(2):
            stride=side*0x97
            mu.mem_write(0x6625c+stride,struct.pack('<I',STS))
            for address,value in [(0x66214,312),(0x66254,312),(0x66258,120),
                                  (0x6623a,120),(0x6622e,robot_id if side==0 else 6)]:
                mu.mem_write(address+stride,struct.pack('<h',value))
            for slot in range(3):
                mu.mem_write(0x70316+side*0x3c+slot*20,struct.pack('<h',-10000))
        return mu

    def run(mu,address):
        mu.mem_write(STACK,struct.pack('<I',STOP))
        mu.reg_write(UC_X86_REG_ESP,STACK)
        mu.reg_write(UC_X86_REG_EAX,0)
        mu.emu_start(address,STOP,count=50000)
        if mu.reg_read(UC_X86_REG_EIP)!=STOP:
            raise ValueError('DOS routine did not return')

    def word(mu,address):
        return struct.unpack('<h',mu.mem_read(address,2))[0]

    commands=[]
    # Execute the actual command matcher and ground/air/super/stolen gates.
    for command in parsed['commands']:
        target=command['target']
        if any(value!=254 and (value>63 or (value&0x21 and value&0x1e)) for value in command['inputs']):
            continue
        mu=machine()
        values=bytes(0 if value==254 else value for value in command['inputs'])
        mu.mem_write(0x685dc,values+bytes([240])*(16-len(values)))
        mu.mem_write(0x62610,struct.pack('<h',1)) # original finishing debug gate
        mu.mem_write(0x66272,struct.pack('<h',24 if target==88 else 0))
        if target>=90: mu.mem_write(0x66284,bytes([1<<(target-90)]))
        if parsed['moves'][target]['state']['ground_mode']==2:
            mu.mem_write(0x66214,struct.pack('<h',200))
        mu.reg_write(UC_X86_REG_EDX,BANK+parsed['moves'][0]['descriptor_offset']+24)
        run(mu,0x234fa)
        actual=word(mu,0x6620e)
        commands.append(dict(target=target,observed=actual))
        if actual!=target:
            earlier=parsed['commands'][:parsed['commands'].index(command)]
            if not any(c['target']==actual and len(c['inputs'])<=len(values) and
                       all(v==254 or v==values[i] for i,v in enumerate(c['inputs'])) for c in earlier):
                raise ValueError(f'Unexpected DOS command: {target} -> {actual}')
            commands[-1]['priority_override']=True

    # Follow a real looping projectile and its impact script without drawing.
    move=parsed['moves'][84]
    row=struct.unpack_from('<5I',bank,int(parsed['dword4'],16)+84*20)
    mu=machine()
    mu.mem_write(0x6620e,struct.pack('<h',84))
    mu.mem_write(0x70316,struct.pack('<h',0))
    mu.mem_write(0x70318,struct.pack('<h',0))
    mu.mem_write(0x70320,struct.pack('<I',BANK+row[3]))
    mu.mem_write(0x70324,struct.pack('<I',BANK+row[4]))
    states=[]
    for tick in range(12):
        mu.reg_write(UC_X86_REG_EDX,0)
        run(mu,0x244eb)
        states.append(dict(image=word(mu,0x70314),x=word(mu,0x70316),y=word(mu,0x70318)))
    if not any(s['image']!=1000 for s in states) or states[-1]['x']<=states[0]['x']:
        raise ValueError('DOS projectile did not become visible and travel')
    return dict(executable_sha256=digest,bank=bank_name,commands=commands,projectile_move=84,projectile=states)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=SOURCE)
    parser.add_argument('--bank',default='RBTF')
    parser.add_argument('--output',type=Path,default=ANALYSIS/'combat_x86.json')
    args=parser.parse_args()
    report=verify(args.source,args.bank)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f"Verified {len(report['commands'])} native commands and twelve projectile ticks")
