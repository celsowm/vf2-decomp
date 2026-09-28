#!/usr/bin/env python3
"""Scan all TGP object-table entries; render largest meshes both decode modes."""
from __future__ import annotations

import struct
from pathlib import Path

# reuse render helpers
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from render_poly_objects import (
    build, MAIN_PAIRS, POLY_PAIRS, TABLE, BASE, OUT,
    decode_object, project, fill_tri, write_ppm, u32, f32,
)


def decode_mode_noskip(poly: bytes, word_index: int, max_links=4096):
    """Alternate hypothesis: no 3-word skip (geometry_mode&3 >= 2)."""
    tris = []
    off = word_index * 4

    def rd():
        nonlocal off
        if off + 4 > len(poly):
            raise EOFError
        v = u32(poly, off)
        off += 4
        return v

    try:
        p0 = (f32(rd()), f32(rd()), f32(rd()))
        p1 = (f32(rd()), f32(rd()), f32(rd()))
    except EOFError:
        return tris
    for _ in range(max_links):
        try:
            attr = rd()
        except EOFError:
            break
        if (attr & 3) == 0:
            break
        try:
            p2 = (f32(rd()), f32(rd()), f32(rd()))
            p3 = (f32(rd()), f32(rd()), f32(rd())) if attr & 1 else None
        except EOFError:
            break
        pts = p0 + p1 + p2 + (p3 or ())
        if all(abs(c) < 1e3 for c in pts):
            tris.append((p0, p1, p2))
            if p3:
                tris.append((p0, p2, p3))
        mode = (attr >> 8) & 3
        if mode in (0, 2):
            p0, p1 = p2, (p3 or p1)
        elif mode == 1:
            p1 = p2
        else:
            p0 = p3 or p2
        if len(tris) > 10000:
            break
    return tris


def render(tris, path: Path, w=320, h=320):
    if not tris:
        return False
    projected, bounds = project(tris, w, h)
    buf = [0] * (w * h)
    for i, tri in enumerate(projected):
        shade = 30 + (i * 53) % 220
        fill_tri(buf, w, h, tri, (shade << 16) | ((shade // 3) << 8) | 255)
    write_ppm(path, w, h, buf)
    return True


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    main_img = build(MAIN_PAIRS, 0x02400000)
    poly = build(POLY_PAIRS, 0x02000000)
    rows = []
    for obj in range(0x200):
        off = (TABLE - BASE) + obj * 16
        if off + 16 > len(main_img):
            break
        a, b, c, d = struct.unpack_from("<IIII", main_img, off)
        if c == 0:
            continue
        word_index = c & 0x7FFFFF
        tris_skip = decode_object(poly, word_index)
        tris_raw = decode_mode_noskip(poly, word_index)
        rows.append((len(tris_skip) + len(tris_raw), obj, c, d, word_index, tris_skip, tris_raw))
    rows.sort(reverse=True)
    print("top objects by triangle counts:")
    for n, obj, c, d, wi, ts, tr in rows[:20]:
        print(f"  id=0x{obj:03x} w2={c:#010x} w3={d:#010x} word={wi:#x} tris_skip={len(ts)} tris_raw={len(tr)}")
    # render top 12 with both modes
    for n, obj, c, d, wi, ts, tr in rows[:12]:
        for label, tris in (("skip", ts), ("raw", tr)):
            if len(tris) < 2:
                continue
            path = OUT / f"scan_{obj:03x}_{label}_{len(tris)}.ppm"
            if render(tris, path):
                print(f"  rendered {path.name}")


if __name__ == "__main__":
    main()
