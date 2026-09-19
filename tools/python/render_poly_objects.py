#!/usr/bin/env python3
"""Software-render measured attract polygon objects to PPM (oracle evidence dump).

Uses the same float-link decode as src/hardware/tgp.c geometry_execute_object.
Output is host-side analysis of measured polygon ROM offsets — not a recovery claim.
"""
from __future__ import annotations

import math
import struct
from pathlib import Path

ROM_DIR = Path("roms/vf2")
OUT = Path("out/attr-logo")
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


def build(pairs, size) -> bytes:
    region = bytearray(size)
    for name, off in pairs:
        p = ROM_DIR / name
        if not p.exists():
            continue
        src = p.read_bytes()
        for i in range(0, len(src), 2):
            dest = off + (i // 2) * 4
            if dest + 1 < len(region):
                region[dest] = src[i]
                region[dest + 1] = src[i + 1]
    return bytes(region)


def u32(blob: bytes, off: int) -> int:
    return struct.unpack_from("<I", blob, off)[0]


def f32(word: int) -> float:
    return struct.unpack("<f", struct.pack("<I", word & 0xFFFFFFFF))[0]


def decode_object(poly: bytes, word_index: int, max_links: int = 4096) -> list[tuple]:
    """Decode using tgp.c: word_address = (w2 & 0x7fffff); byte = word*4.

    geometry_execute_object: 6 floats (p0/p1), then loop attr; if (attr&3)==0
    break; if geometry_mode&3 < 2 skip 3 words; read p2; render; update p0/p1.
    """
    tris = []
    off = word_index * 4

    def rd() -> int:
        nonlocal off
        if off < 0 or off + 4 > len(poly):
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
            rd(); rd(); rd()  # mode<2 skip
            p2 = (f32(rd()), f32(rd()), f32(rd()))
            if attr & 1:
                p3 = (f32(rd()), f32(rd()), f32(rd()))
            else:
                p3 = None
        except EOFError:
            break
        pts = p0 + p1 + p2 + (p3 or ())
        if all(abs(c) < 1e6 for c in pts):
            tris.append((p0, p1, p2))
            if p3 is not None:
                tris.append((p0, p2, p3))
        mode = (attr >> 8) & 3
        if mode in (0, 2):
            p0, p1 = p2, (p3 if p3 is not None else p1)
        elif mode == 1:
            p1 = p2
        else:
            p0 = p3 if p3 is not None else p2
        if len(tris) > 8000:
            break
    return tris


def project(tris, w=256, h=256):
    pts = [p for t in tris for p in t]
    if not pts:
        return []
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    zs = [p[2] for p in pts]
    minx, maxx = min(xs), max(xs)
    miny, maxy = min(ys), max(ys)
    minz, maxz = min(zs), max(zs)
    sx = (w - 16) / max(maxx - minx, 1e-6)
    sy = (h - 16) / max(maxy - miny, 1e-6)
    s = min(sx, sy)

    def to_screen(p):
        x = int((p[0] - minx) * s) + 8
        y = int((maxy - p[1]) * s) + 8
        z = p[2]
        return x, y, z

    out = []
    for t in tris:
        out.append(tuple(to_screen(p) for p in t))
    return out, (minx, maxx, miny, maxy, minz, maxz)


def fill_tri(buf, w, h, tri, color):
    (x0, y0, z0), (x1, y1, z1), (x2, y2, z2) = tri
    minx = max(0, min(x0, x1, x2))
    maxx = min(w - 1, max(x0, x1, x2))
    miny = max(0, min(y0, y1, y2))
    maxy = min(h - 1, max(y0, y1, y2))
    area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
    if area == 0:
        return
    for y in range(miny, maxy + 1):
        for x in range(minx, maxx + 1):
            w0 = (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0)
            w1 = (x2 - x1) * (y - y1) - (y2 - y1) * (x - x1)
            w2 = (x0 - x2) * (y - y2) - (y0 - y2) * (x - x2)
            if (w0 >= 0 and w1 >= 0 and w2 >= 0) or (w0 <= 0 and w1 <= 0 and w2 <= 0):
                buf[y * w + x] = color


def write_ppm(path: Path, w: int, h: int, buf: list[int]) -> None:
    raw = bytearray()
    for c in buf:
        raw += bytes([(c >> 16) & 255, (c >> 8) & 255, c & 255])
    path.write_bytes(f"P6\n{w} {h}\n255\n".encode() + raw)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    main_img = build(MAIN_PAIRS, 0x02400000)
    poly = build(POLY_PAIRS, 0x02000000)
    ids = [0x88, 0x140, 0x144, 0x148, 0x149, 0x14f, 0x150, 0x157, 0x97d, 0x985]
    for obj in ids:
        off = (TABLE - BASE) + obj * 16
        a, b, c, d = struct.unpack_from("<IIII", main_img, off)
        # TGP: word_address = object_address & 0x7fffff; byte = word * 4
        word_index = c & 0x7FFFFF
        tris = decode_object(poly, word_index)
        print(
            f"id=0x{obj:03x} w2={c:#010x} word={word_index:#x} "
            f"byte={word_index*4:#x} tris={len(tris)} w3={d:#010x}"
        )
        if not tris:
            continue
        projected, bounds = project(tris)
        w = h = 256
        buf = [0] * (w * h)
        for i, tri in enumerate(projected):
            shade = 40 + (i * 37) % 200
            fill_tri(buf, w, h, tri, (shade << 16) | ((shade // 2) << 8) | 255)
        path = OUT / f"obj_{obj:03x}_w{word_index:x}.ppm"
        write_ppm(path, w, h, buf)
        print(f"  wrote {path} bounds={tuple(round(x,4) for x in bounds)}")


if __name__ == "__main__":
    main()
