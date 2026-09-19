#!/usr/bin/env python3
"""Find ROM call sites of helper 0x7c60 and nearby object-id loads."""
from __future__ import annotations

import struct
import subprocess
from pathlib import Path

I960 = "build/Debug/vf2i960.exe"
ROM = "roms/vf2"


def build_maincpu() -> bytes:
    ROM_DIR = Path("roms/vf2")
    region = bytearray(0x200000)
    for name, off in (
        ("epr-18385.12", 0),
        ("epr-18386.13", 2),
        ("epr-18387.14", 0x40000),
        ("epr-18388.15", 0x40002),
    ):
        src = (ROM_DIR / name).read_bytes()
        for i in range(0, len(src), 2):
            di = off + i * 2
            if di + 1 < len(region):
                region[di] = src[i]
                region[di + 1] = src[i + 1]
    return bytes(region)


def main() -> None:
    img = build_maincpu()
    # i960 call encoding: 0x09xxxxxx relative? From disasm: 09fe61c4 call 0x7c60 at 0x21a9c
    # target = ip + 8 + sign_extend(imm24)?  0x21a9c + 8 + disp = 0x7c60
    # disp = 0x7c60 - 0x21aa4 = negative
    # Search for call words that land on 0x7c60
    hits = []
    for ip in range(0, len(img) - 4, 4):
        word = struct.unpack_from("<I", img, ip)[0]
        if (word >> 24) != 0x09:
            continue
        disp = word & 0xFFFFFF
        if disp & 0x800000:
            disp -= 0x1000000
        # i960 COBR/ctrl call: typically ip+4+disp or ip+8+disp — try both
        for base in (ip + 4, ip + 8):
            tgt = (base + disp) & 0xFFFFFFFF
            if tgt == 0x00007C60:
                hits.append((ip, base, disp, tgt))
    print(f"call 0x7c60 candidates: {len(hits)}")
    for ip, base, disp, tgt in hits[:40]:
        print(f"  @0x{ip:08x} base=0x{base:08x} disp={disp:#x} -> 0x{tgt:08x}")
        # disasm 8 instructions before
        p = subprocess.run(
            [I960, "disasm", ROM, hex(max(0, ip - 24)), "10"],
            capture_output=True, text=True,
        )
        for ln in (p.stdout + p.stderr).splitlines():
            if ln.strip():
                print("   ", ln)


if __name__ == "__main__":
    main()
