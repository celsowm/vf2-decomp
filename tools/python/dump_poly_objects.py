#!/usr/bin/env python3
"""Dump polygons.bin at object-table w2 offsets; search poly/main_data for SEGA."""
from __future__ import annotations

import struct
from pathlib import Path

ROM_DIR = Path("roms/vf2")
BASE = 0x02000000
TABLE = 0x020E0004

MAIN_PAIRS = [
    ("mpr-17560.10", 0x00000000), ("mpr-17561.11", 0x00000002),
    ("mpr-17558.8", 0x00400000), ("mpr-17559.9", 0x00400002),
    ("mpr-17566.6", 0x00800000), ("mpr-17567.7", 0x00800002),
    ("mpr-17564.4", 0x00C00000), ("mpr-17565.5", 0x00C00002),
]
POLY_PAIRS = [
    ("mpr-17554.16", 0x00000000), ("mpr-17548.20", 0x00000002),
    ("mpr-17555.17", 0x00400000), ("mpr-17549.21", 0x00400002),
    ("mpr-17556.18", 0x00800000), ("mpr-17550.22", 0x00800002),
]


def build(pairs, size=0x02000000) -> bytes:
    region = bytearray(size)
    for name, off in pairs:
        p = ROM_DIR / name
        if not p.exists():
            print("missing", p)
            continue
        src = p.read_bytes()
        for i in range(0, len(src), 2):
            dest = off + (i // 2) * 4
            if dest + 1 < len(region):
                region[dest] = src[i]
                region[dest + 1] = src[i + 1]
    return bytes(region)


def find_ascii(blob: bytes, needle: bytes, limit=20) -> list[int]:
    hits = []
    start = 0
    while len(hits) < limit:
        i = blob.find(needle, start)
        if i < 0:
            break
        hits.append(i)
        start = i + 1
    return hits


def main() -> None:
    main_img = build(MAIN_PAIRS, 0x02400000)
    poly = build(POLY_PAIRS, 0x02000000)
    print(f"main_data={len(main_img):#x} polygons={len(poly):#x}")

    print("\n=== ASCII search SEGA/logo/title in regions ===")
    for label, blob in (("main_data", main_img), ("polygons", poly)):
        for nd in (b"SEGA", b"Sega", b"sega", b"LOGO", b"TITLE", b"VF2", b"VIRTUA"):
            hits = find_ascii(blob, nd, 8)
            if hits:
                print(f"  {label} {nd!r}: {[hex(h) for h in hits]}")
                for h in hits[:3]:
                    ctx = blob[h : h + 32]
                    print(f"    @{h:#x}: {ctx!r}")

    print("\n=== polygon ROM at attract object w2 offsets ===")
    ids = [0x88, 0x140, 0x144, 0x145, 0x148, 0x149, 0x14f, 0x150, 0x157, 0x97d, 0x985]
    for obj in ids:
        off = (TABLE - BASE) + obj * 16
        a, b, c, d = struct.unpack_from("<IIII", main_img, off)
        rom_bit = bool(c & 0x00800000)
        poff = c & 0x7FFFFF
        print(
            f"  id=0x{obj:03x} w0={a:#010x} w1={b:#010x} w2={c:#010x} "
            f"w3={d:#010x} poly_rom={rom_bit} poly_off={poff:#x}"
        )
        if rom_bit and poff + 32 <= len(poly):
            words = [struct.unpack_from("<I", poly, poff + i)[0] for i in range(0, 32, 4)]
            print(f"    poly[poff]={ [hex(w) for w in words] }")

    # scan polygons for object command class 0x01-like headers
    print("\n=== polygons.bin word histogram of class bits (sample) ===")
    from collections import Counter
    cls = Counter()
    for i in range(0, min(len(poly), 0x200000), 4):
        w = struct.unpack_from("<I", poly, i)[0]
        cls[(w >> 23) & 0x1F] += 1
    print("  class top", cls.most_common(12))


if __name__ == "__main__":
    main()
