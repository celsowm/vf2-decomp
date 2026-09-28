#!/usr/bin/env python3
"""Build main_data (LOAD32_WORD) and dump phase15/selector0 descriptors."""
from __future__ import annotations

from pathlib import Path

ROM_DIR = Path("roms/vf2")
MAIN_DATA_SIZE = 0x02400000
BASE = 0x02000000

PAIRS = [
    ("mpr-17560.10", 0x00000000),
    ("mpr-17561.11", 0x00000002),
    ("mpr-17558.8", 0x00400000),
    ("mpr-17559.9", 0x00400002),
    ("mpr-17566.6", 0x00800000),
    ("mpr-17567.7", 0x00800002),
    ("mpr-17564.4", 0x00C00000),
    ("mpr-17565.5", 0x00C00002),
]

ADDRESSES = [
    0x02A69CD2,
    0x02A69E4A,
    0x02A69EE6,
    0x02A69F4C,
    0x02A69E80,
    0x02A69F02,
    0x02A6845A,
    0x02A6C0DA,
    0x02A6C15E,
    0x02A68586,
    0x02A6D8AA,
    0x02A6F24E,
    0x02A6F606,
    0x02A6F43A,
]


def load32_word(region: bytearray, src: bytes, offset: int) -> None:
    for index in range(0, len(src), 2):
        dest = offset + (index // 2) * 4
        if dest + 1 < len(region):
            region[dest] = src[index]
            region[dest + 1] = src[index + 1]


def build() -> bytes:
    region = bytearray(MAIN_DATA_SIZE)
    for name, off in PAIRS:
        p = ROM_DIR / name
        if not p.exists():
            print("missing", p)
            continue
        load32_word(region, p.read_bytes(), off)
    return bytes(region)


def decode_glyphs(buf: bytes, n: int = 80) -> str:
    chars = []
    for i in range(0, min(len(buf), n) - 1, 2):
        v = buf[i] | (buf[i + 1] << 8)
        hi = v & 0xFF00
        if hi in (0x8000, 0x8900) and 32 <= (v & 0xFF) < 127:
            chars.append(chr(v & 0xFF))
        elif v == 0:
            chars.append(" ")
        else:
            chars.append(".")
    return "".join(chars)


def main() -> None:
    img = build()
    print(f"main_data len=0x{len(img):x}")
    for addr in ADDRESSES:
        off = addr - BASE
        if off < 0 or off + 16 > len(img):
            print(f"  0x{addr:08x}: OOB off=0x{off:x}")
            continue
        chunk = img[off : off + 96]
        # descriptor header: s16 addend, u16 mode, then rows/cols at +4/+8 per notes
        addend = int.from_bytes(chunk[0:2], "little", signed=True)
        mode = int.from_bytes(chunk[2:4], "little")
        rows = int.from_bytes(chunk[4:8], "little")
        cols = int.from_bytes(chunk[8:12], "little")
        payload = chunk[12:]
        print(
            f"  0x{addr:08x} off=0x{off:x} addend={addend} mode={mode} "
            f"rows={rows} cols={cols} head={chunk[:16].hex()}"
        )
        print(f"    glyphs96={decode_glyphs(chunk, 96)!r}")
        print(f"    payload64={decode_glyphs(payload, 64)!r} phex={payload[:24].hex()}")


if __name__ == "__main__":
    main()
