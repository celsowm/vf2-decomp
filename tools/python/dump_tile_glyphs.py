#!/usr/bin/env python3
"""Decode tile-plane glyphs including 0x88xx/0x89xx banks from attract parks."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, wu8, wu32

BANKS = {0x8000, 0x8800, 0x8900}


def decode_tile(tile: bytes, base: int, length: int, limit: int = 400) -> str:
    chars = []
    for i in range(0, min(len(tile), length) - 1, 2):
        addr = base + i
        v = tile[i] | (tile[i + 1] << 8)
        hi = v & 0xFF00
        ch = v & 0xFF
        if hi in BANKS and 32 <= ch < 127:
            chars.append(chr(ch))
        elif hi in BANKS:
            chars.append(f"[{hi|ch:04x}]")
        else:
            if chars and chars[-1] != "|":
                chars.append("|")
    s = "".join(chars)
    while "||" in s:
        s = s.replace("||", "|")
    return s[:limit]


def dump_region(path: Path, start: int, length: int) -> None:
    s = parse_snap(path)
    w = s["regions"]["work-ram"]
    tile = s["regions"]["tile-ram"]
    # tile-ram base in model2 is typically 0x01000000
    # snapshot region is raw tile RAM; address in plane maps as offset
    # Probe writes used absolute 0x01000xxx which is video/tile space.
    # dump_attract_state tile_strings walks raw region with 0x80xx assumption.
    off = start - 0x01000000
    if off < 0 or off >= len(tile):
        # try as raw offset into tile region
        off = start if start < len(tile) else 0
    chunk = tile[off : off + length]
    print(f"\n== {path.name} ph=0x{wu8(w,0x500030):02x} mask=0x{wu32(w,0x500028):08x} "
          f"plane@0x{start:08x}+{length}")
    print(f"  glyphs: {decode_tile(chunk, start, length)!r}")
    # also full-tile multi-bank
    print(f"  full88: {decode_tile(tile, 0x01000000, min(len(tile), 0x4000))!r}")


def main() -> None:
    paths = [
        Path("out/attr-fs/all0-ready1.vf2snap"),
        Path("out/attr-tail/p15-mask700.vf2snap"),
        Path("out/attr-tail/p15-mask380.vf2snap"),
        Path("out/attr-tail/p15-mask1c0.vf2snap"),
        Path("out/attr-tail/p15-mask540.vf2snap"),
        Path("out/attr-tail/p14-thunk-body.vf2snap"),
        Path("out/attr-tail/long-ready0.vf2snap"),
    ]
    windows = [
        (0x01000124, 0x200),
        (0x01000700, 0x200),
        (0x01000000, 0x400),
        (0x01000ef4, 0x80),
    ]
    for p in paths:
        if not p.exists():
            print("missing", p)
            continue
        for start, length in windows:
            dump_region(p, start, length)


if __name__ == "__main__":
    main()
