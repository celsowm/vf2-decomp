#!/usr/bin/env python3
"""Decode attract jump tables from extracted maincpu (word LE in file)."""
from __future__ import annotations

import struct
import sys
from pathlib import Path

img = Path(sys.argv[1] if len(sys.argv) > 1 else "out/maincpu.bin").read_bytes()


def u32le(addr: int) -> int:
    return struct.unpack_from("<I", img, addr)[0]


print("=== frame selector table 0xa6f8 (logical addr = LE u32) ===")
for i in range(20):
    v = u32le(0xA6F8 + i * 4)
    print(f"  sel[{i:02d}] = 0x{v:08x}")

print("\n=== sel3 phase table 0xaac4 ===")
for i in range(18):
    v = u32le(0xAAC4 + i * 4)
    print(f"  phase[{i:02d}] = 0x{v:08x}")
