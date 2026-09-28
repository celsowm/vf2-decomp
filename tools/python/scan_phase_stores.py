#!/usr/bin/env python3
"""Scan built maincpu for absolute stores targeting 0x00500030 / nearby."""
from __future__ import annotations

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))


def build_maincpu() -> bytes:
    from dump_sel0_strings import build_maincpu
    return build_maincpu()


def main() -> None:
    img = build_maincpu()
    print(f"maincpu len=0x{len(img):x}")
    targets = {
        0x00500030: "phase",
        0x00500028: "mask28",
        0x0050002A: "sel",
        0x00550000: "ready",
        0x00500024: "cd",
    }
    # st / stib / stos encodings often embed abs addr as LE word
    for addr, name in targets.items():
        needle = struct.pack("<I", addr)
        hits = []
        start = 0
        while True:
            i = img.find(needle, start)
            if i < 0:
                break
            # show surrounding instruction-like words
            ctx = []
            for back in range(0, 16, 4):
                off = i - back
                if off >= 0:
                    ctx.append(img[off : off + 4].hex())
            hits.append((hex(i), ctx))
            start = i + 1
            if len(hits) >= 40:
                break
        print(f"\n{name} 0x{addr:08x} needle={needle.hex()} hits={len(hits)}")
        for h in hits[:20]:
            print(f"  @ {h[0]} ctx_le={[c for c in h[1]]}")

    # Also scan for COBR/ctrl stib-like: opcode c2 followed by addr
    print("\n=== scan stib-like c2 + phase word ===")
    phase = struct.pack("<I", 0x00500030)
    # some encodings: c2 XX 30 00 50 00 or 30 00 50 00 after c2
    for i in range(len(img) - 6):
        if img[i] == 0xC2 and img[i + 2 : i + 6] == phase:
            print(f"  stib-cand @ {i:08x} bytes={img[i:i+8].hex()}")
        if img[i : i + 4] == phase and i >= 1 and img[i - 1] in (0xC2, 0x8A, 0x92, 0x82):
            op = img[i - 1]
            print(f"  store-cand op={op:02x} @ {i-1:08x} bytes={img[i-1:i+5].hex()}")


if __name__ == "__main__":
    main()
