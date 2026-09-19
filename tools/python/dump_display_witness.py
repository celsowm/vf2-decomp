#!/usr/bin/env python3
"""Display/image witness from a .vf2snap (host-side, no Model2 raster)."""
from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_snap_video import read_snapshot, u32, u64  # noqa: E402


def crc32_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()[:16]


def tile_head(tile: bytes, limit: int = 96) -> str:
    chars = []
    for i in range(0, min(len(tile), 0x4000) - 1, 2):
        v = tile[i] | (tile[i + 1] << 8)
        if (v & 0xFF00) == 0x8000 and 32 <= (v & 0xFF) < 127:
            chars.append(chr(v & 0xFF))
        if len(chars) >= limit:
            break
    return "".join(chars)


def geometry_stats(geom: bytes) -> dict:
    # count non-zero dwords and first tags
    nz = 0
    tags = []
    for i in range(0, min(len(geom), 0x8000) - 3, 4):
        w = u32(geom, i)
        if w != 0:
            nz += 1
            if len(tags) < 12:
                tags.append(f"{w:08x}")
    return {
        "len": len(geom),
        "sha16": crc32_bytes(geom),
        "nz_dwords_32k": nz,
        "first_tags": tags,
    }


def buffer_stats(buf: bytes) -> dict:
    return {
        "len": len(buf),
        "sha16": crc32_bytes(buf),
        "head16": buf[:16].hex(),
    }


def texture_stats(tex: bytes | None) -> dict | None:
    if tex is None:
        return None
    nz = sum(1 for b in tex[:0x10000] if b != 0 and b != 0xFF)
    return {
        "len": len(tex),
        "sha16": crc32_bytes(tex[:0x10000]),
        "nz_in_first_64k": nz,
        "head8": tex[:8].hex(),
    }


def witness(path: Path) -> dict:
    snap = read_snapshot(path)
    work = snap["regions"]["work-ram"]
    tile = snap["regions"]["tile-ram"]
    geom = snap["regions"]["geometry"]
    buf = snap["regions"]["buffer-ram"]
    tex0 = snap["regions"].get("texture-ram0")
    tex1 = snap["regions"].get("texture-ram1")
    pal = snap["regions"].get("palette-ram")

    def wu32(a):
        return u32(work, a - 0x500000)

    def wu8(a):
        return work[a - 0x500000]

    head = tile_head(tile)
    return {
        "file": path.name,
        "ip": f"{snap['ip']:08x}",
        "executed": snap.get("executed_instructions"),
        "sel_50002a": wu8(0x50002A),
        "sel_50002b": wu8(0x50002B),
        "a4_5000a4": wu8(0x5000A4),
        "a5_5000a5": wu8(0x5000A5),
        "countdown_500024": wu32(0x500024),
        "flags_500068": f"{wu32(0x500068):08x}",
        "nav_500704": f"{wu32(0x500704):08x}",
        "tile_sha16": crc32_bytes(tile),
        "tile_head": head,
        "tile_has_TEST": "TEST" in head.upper(),
        "tile_has_EXIT": "EXIT" in head.upper(),
        "tile_has_BACKUP": "BACKUP" in head.upper(),
        "tile_has_SEGA": "SEGA" in head.upper(),
        "geometry": geometry_stats(geom),
        "buffer": buffer_stats(buf),
        "texture0": texture_stats(tex0),
        "texture1": texture_stats(tex1),
        "palette_sha16": crc32_bytes(pal) if pal else None,
        "registers_g": [f"{snap['registers'][16+i]:08x}" for i in range(8)],
    }


def main() -> None:
    out = []
    for arg in sys.argv[1:]:
        p = Path(arg)
        if not p.exists():
            print(f"missing {p}", file=sys.stderr)
            continue
        w = witness(p)
        out.append(w)
        print(json.dumps(w, ensure_ascii=False))
    if len(out) >= 2:
        base = out[0]
        print("=== DIFF vs", base["file"], "===", file=sys.stderr)
        for w in out[1:]:
            print(f"-- {w['file']}", file=sys.stderr)
            for k in (
                "sel_50002a",
                "a4_5000a4",
                "a5_5000a5",
                "countdown_500024",
                "flags_500068",
                "tile_sha16",
                "tile_head",
            ):
                if base.get(k) != w.get(k):
                    print(f"  {k}: {base.get(k)!r} -> {w.get(k)!r}", file=sys.stderr)
            for sec in ("geometry", "buffer", "texture0"):
                if base.get(sec) and w.get(sec):
                    if base[sec].get("sha16") != w[sec].get("sha16"):
                        print(
                            f"  {sec}.sha16: {base[sec].get('sha16')} -> {w[sec].get('sha16')}",
                            file=sys.stderr,
                        )


if __name__ == "__main__":
    main()
