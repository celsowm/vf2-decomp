#!/usr/bin/env python3
"""Deep image witness: full-region hashes + non-zero density + tile strings."""
from __future__ import annotations

import hashlib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_snap_video import read_snapshot, u32

REGIONS = (
    "geometry",
    "buffer-ram",
    "texture-ram0",
    "texture-ram1",
    "tile-ram",
    "palette-ram",
    "luma-ram",
    "copro-port",
    "video-control",
    "color-translation",
)


def sha16(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()[:16]


def density(b: bytes, sample: int = 256 * 1024) -> tuple[int, int]:
    chunk = b[:sample]
    nz = sum(1 for x in chunk if x not in (0, 0xFF))
    return nz, len(chunk)


def tile_strings(tile: bytes) -> str:
    chars = []
    for i in range(0, min(len(tile), 0x8000) - 1, 2):
        v = tile[i] | (tile[i + 1] << 8)
        if (v & 0xFF00) == 0x8000 and 32 <= (v & 0xFF) < 127:
            chars.append(chr(v & 0xFF))
        else:
            if chars and chars[-1] != "|":
                chars.append("|")
    s = "".join(chars)
    while "||" in s:
        s = s.replace("||", "|")
    return s[:200]


def main() -> None:
    for arg in sys.argv[1:]:
        p = Path(arg)
        snap = read_snapshot(p)
        print(f"\n== {p.name} ip={snap['ip']:08x}")
        work = snap["regions"]["work-ram"]
        print(
            f"  sel={work[0x2a]:02x} a4={work[0xa4]:02x} "
            f"flags={u32(work, 0x68):08x}"
        )
        print("  tiles:", tile_strings(snap["regions"]["tile-ram"]))
        for name in REGIONS:
            data = snap["regions"].get(name)
            if data is None:
                print(f"  {name}: MISSING")
                continue
            nz, n = density(data)
            print(f"  {name}: len={len(data)} sha={sha16(data)} nz~{nz}/{n}")


if __name__ == "__main__":
    main()
