"""Check DOS attack filtering, jump arithmetic and crossing against the user's executable.

Optional dependency: pip install --target ANALYSIS/verification_deps unicorn
Only the analysed older DOS EXR is mapped. No original code or assets embedded.
Joystick polling and landing effects are skipped; input and physics run unchanged.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

from extract_mvs import parse_bank
from le_codec import flatten
from project_paths import ROOT, SOURCE, ANALYSIS

sys.path.insert(0, str(ROOT / "ANALYSIS/verification_deps"))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP

STOP, STACK, DESCRIPTOR = 0x1000, 0xf00000, 0x800000
EXR_SHA = "213ba86c2292cb3203f87826c030fbdb9d4c77c3a8c27d69fce230a53dc4241b"


def verify(source):
    raw = (source / "RISE2.EXR").read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != EXR_SHA:
        raise ValueError("Unmapped EXR: verify its addresses in Ghidra first.")
    image, metadata = flatten(raw)

    def machine():
        mu = Uc(UC_ARCH_X86, UC_MODE_32)
        mu.mem_map(0, 0x1000000)
        mu.mem_write(metadata["base"], image)
        def skip(mu, address, size, unused):
            if address == 0x199b5:  # joystick polling in FUN_197b6
                mu.reg_write(UC_X86_REG_EIP, 0x199ba)
            elif address == 0x2340a:  # landing effects in FUN_231ac
                mu.reg_write(UC_X86_REG_EIP, 0x2340f)
        mu.hook_add(UC_HOOK_CODE, skip)
        return mu

    def run(mu, entry):
        mu.mem_write(STACK, struct.pack("<I", STOP))
        mu.reg_write(UC_X86_REG_ESP, STACK)
        mu.emu_start(entry, STOP, count=20000)
        if mu.reg_read(UC_X86_REG_EIP) != STOP:
            raise ValueError("DOS function did not return.")

    def word(mu, address):
        return struct.unpack("<h", mu.mem_read(address, 2))[0]

    mu = machine()
    mu.mem_write(0x67a70, bytes(4))
    attacks = []
    for raw_keys in [0x10, 0x10, 0x80, 0, 0x80]:
        mu.mem_write(0x5097a, struct.pack("<I", raw_keys))
        run(mu, 0x197b6)
        attacks.append(word(mu, 0x65fe0) & 0x3f)
    if attacks != [1, 0, 0, 0, 32]:
        raise ValueError(f"Unexpected DOS attack filtering: {attacks}")
    mu.mem_write(0x66224, b"\x01")
    mu.mem_write(0x5097a, struct.pack("<I", 8))  # screen right, facing left
    run(mu, 0x197b6)
    if word(mu, 0x65fe0) & 6 != 4:
        raise ValueError("DOS horizontal inputs were not made facing-relative.")

    move = parse_bank(source / "RBTF.MVS")["moves"][32]
    mu = machine()
    for address in [0x66214, 0x6621e, 0x66254]:
        mu.mem_write(address, struct.pack("<h", 312))
    impulse = struct.unpack("b", bytes([move["param"]]))[0]
    mu.mem_write(0x6621c, struct.pack("<h", impulse * 256))
    gravity = move["state"]["gravity"] * 3 // 4
    mu.mem_write(0x66252, struct.pack("<h", gravity))
    mu.mem_write(0x66222, bytes(2))
    mu.mem_write(DESCRIPTOR, bytes(28) + bytes([int(move["flags"], 16),
                                             move["auto_move"], move["resume_index"], move["param"]]))
    heights = []
    for tick in range(100):
        mu.reg_write(UC_X86_REG_EAX, 0)
        mu.reg_write(UC_X86_REG_EDX, DESCRIPTOR)
        run(mu, 0x231ac)
        heights.append(word(mu, 0x66214))
        if heights[-1] == 312:
            break
    else:
        raise ValueError("DOS jump failed to land.")
    landing_move = word(mu, 0x6620e)

    # FUN_25615 and its real FUN_26321 helper: relocate a supplied MVS bank
    # exactly as the loader does, so sequence clamping also executes unchanged.
    bank_address = DESCRIPTOR + 0x10000
    bank = bytearray((source / "RBTF.MVS").read_bytes())
    moves = parse_bank(source / "RBTF.MVS")["moves"]
    for state in moves:
        offset = state["descriptor_offset"]
        for field in range(7):
            pointer = struct.unpack_from("<I", bank, offset + field * 4)[0]
            struct.pack_into("<I", bank, offset + field * 4, bank_address + pointer)
    mu = machine()
    mu.mem_write(bank_address, bytes(bank))
    mu.mem_write(0x685cc, struct.pack("<II", bank_address + 12, bank_address + 12))
    mu.mem_write(0x703b2, struct.pack("<I", 0xffff0000))
    cases = []
    for before, after, position in [(3, 68, 7), (16, 69, 0), (8, 8, 2),
                                    (32, 32, 2), (34, 35, 7), (35, 34, 7),
                                    (35, 34, 40), (37, 37, 1)]:
        for facing in [1, -1]:
            mu.mem_write(0x6620c, bytes(0x97 * 2))
            for player, state, x, direction in [(0, before, 340 if facing == 1 else 300, facing),
                                               (1, 8, 320, facing)]:
                stride = player * 0x97
                for address, value in [(0x6620e, state), (0x66212, x), (0x66214, 200),
                                       (0x66254, 312), (0x66216, position), (0x6621c, -2000)]:
                    mu.mem_write(address + stride, struct.pack("<h", value))
                mu.mem_write(0x66224 + stride, bytes([int(direction < 0)]))
            run(mu, 0x25615)
            observed = dict(before=before, after=word(mu, 0x6620e), facing_before=facing,
                            facing_after=-1 if mu.mem_read(0x66224, 1)[0] & 1 else 1,
                            frame=word(mu, 0x66216), y=word(mu, 0x66214),
                            velocity=word(mu, 0x6621c))
            wanted_frame = min(position, len(moves[after]["sequences"][0]) - 2)
            if after in [68, 69]:
                wanted_frame = 0
            if before in [8, 37]:
                wanted_frame = position
            if observed != dict(before=before, after=after, facing_before=facing,
                                 facing_after=facing if before in [8, 37] else -facing,
                                 frame=wanted_frame, y=200, velocity=-2000):
                raise ValueError(f"Unexpected DOS crossing: {observed}")
            cases.append(observed)
    return dict(executable_sha256=digest, attacks=attacks, facing_left_right_mask=4,
                jump=dict(bank="RBTF", move=32, impulse=impulse, gravity_byte=move["state"]["gravity"],
                          gravity_fixed=gravity, y=heights, landing_move=landing_move),
                crossing=cases)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=SOURCE)
    parser.add_argument("--output", type=Path, default=ANALYSIS / "fighter_x86.json")
    args = parser.parse_args()
    report = verify(args.source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
