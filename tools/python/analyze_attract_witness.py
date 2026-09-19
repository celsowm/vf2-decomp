#!/usr/bin/env python3
"""Summarize attract parks + classify TGP FIFO writes from memory-trace."""
from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, wu8, wu32, tile_strings, WORK_BASE


def glyphs(tile: bytes, limit: int = 200) -> str:
    chars = []
    for i in range(0, min(len(tile), 0x8000) - 1, 2):
        v = tile[i] | (tile[i + 1] << 8)
        if (v & 0xFF00) in (0x8000, 0x8900) and 32 <= (v & 0xFF) < 127:
            chars.append(chr(v & 0xFF))
        else:
            if chars and chars[-1] != "|":
                chars.append("|")
    s = "".join(chars)
    while "||" in s:
        s = s.replace("||", "|")
    return s[:limit]


def geom_stats(geom: bytes) -> dict:
    nz = 0
    tags = {}
    for i in range(0, min(len(geom), 0x8000) - 3, 4):
        w = int.from_bytes(geom[i : i + 4], "little")
        if w != 0:
            nz += 1
            hi = (w >> 24) & 0xFF
            tags[hi] = tags.get(hi, 0) + 1
    top = sorted(tags.items(), key=lambda x: -x[1])[:8]
    return {
        "sha": hashlib.sha256(geom).hexdigest()[:16],
        "nz": nz,
        "top_hi_bytes": top,
    }


def dump(path: Path) -> None:
    s = parse_snap(path)
    w = s["regions"]["work-ram"]
    t = s["regions"]["tile-ram"]
    g = s["regions"]["geometry"]
    b = s["regions"]["buffer-ram"]
    x = s["regions"]["texture-ram0"]
    cfg = wu32(w, 0x50016C)
    print(
        f"{path.name} ip=0x{s['ip']:08x} sel=0x{wu8(w,0x50002A):02x} "
        f"ph=0x{wu8(w,0x500030):02x} a4=0x{wu8(w,0x5000A4):02x} "
        f"nav=0x{wu32(w,0x500704):08x} board=0x{wu32(w,0x508000):08x} "
        f"cd={wu32(w,0x500024)} country={wu8(w,cfg+0x3350) if cfg>=WORK_BASE else -1}"
    )
    print("  tiles80xx:", tile_strings(t, 100))
    print("  glyphs89:", glyphs(t))
    print("  geom", geom_stats(g))
    print(
        "  buf",
        hashlib.sha256(b).hexdigest()[:16],
        "tex0",
        hashlib.sha256(x).hexdigest()[:16],
        "nz_tex64k",
        sum(1 for c in x[:0x10000] if c not in (0, 0xFF)),
    )


def classify_fifo(trace: Path) -> None:
    funcs = {}
    floatish = 0
    writes = 0
    for ln in trace.read_text(errors="replace").splitlines():
        if not ln.startswith("{"):
            continue
        try:
            j = json.loads(ln)
        except Exception:
            continue
        if j.get("type") != "memory" or j.get("kind") != "write":
            continue
        addr = j.get("address")
        if addr != 0x00884000:
            continue
        writes += 1
        try:
            val = int(j.get("bytes", "0"), 16)
            # LE bytes hex string from probe: reverse if needed
            bs = bytes.fromhex(j.get("bytes", "00000000"))
            val = int.from_bytes(bs, "little")
        except Exception:
            continue
        fn = (val >> 23) & 0x3F
        funcs[fn] = funcs.get(fn, 0) + 1
        exp = (val >> 23) & 0xFF
        if 0x7C <= exp <= 0x84 or 0x3C <= exp <= 0x44:
            floatish += 1
    print(f"FIFO writes@884000: {writes} function_codes={sorted(funcs.items(), key=lambda x:-x[1])[:16]} floatish~{floatish}")


def main() -> None:
    for p in sys.argv[1:]:
        if p.endswith(".jsonl"):
            classify_fifo(Path(p))
        else:
            dump(Path(p))


if __name__ == "__main__":
    main()
