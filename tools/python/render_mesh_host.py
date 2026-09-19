#!/usr/bin/env python3
"""Host-side multi-view mesh rasterizer for measured polygon-ROM objects.

ANALYSIS TOOL — not recovered game C, not a logo/title witness.

Geometry decode mirrors src/hardware/tgp.c geometry_execute_object:
  - 6 floats p0(xyz) p1(xyz)
  - loop: attr = rd(); if (attr & 3) == 0 break
  - if geometry_mode&3 < 2: skip 3 words (skip3); else skip 0 (noskip)
  - p2 = rd xyz; if attr&1: p3 = rd xyz
  - render p0,p1,p2; if p3: also p0,p2,p3
  - p0/p1 update by (attr>>8)&3 (tgp.c cases 0/2/1/3)
Object table: main_data LOAD32_WORD pairs; TABLE 0x020E0004; record 16 bytes
at (TABLE-BASE)+id*16. word_index = w2 & 0x7fffff; poly byte = word_index*4.

Projection formulas (ANALYSIS convenience only — not recovered TGP raster):

  fit-ortho / front / top / side / iso-*:
    project vertex onto a fixed camera basis (right, up, depth_into_scene),
    then auto-fit projected (sx, sy) bbox into the output image.
    z-buffer keeps the smallest depth_into_scene.

  tgp:
    apply geometry_transform_point from tgp.c when --matrix/--transforms given:
      x' = (m0*x + m4*y + m8*z + m12*w) * focus_x
      y' = (m1*x + m5*y + m9*z + m13*w) * focus_y
      z' =  m2*x + m6*y + m10*z + m14*w
      w' =  m3*x + m7*y + m11*z + m15*w
    then orthographic fit on (x', y') with depth = z'. If matrix/transforms
    are missing, fall back to identity + focus 1.0 and label the view clearly.

  persp:
    simple host perspective looking down +Z:
      x' = focus * x / z_safe
      y' = focus * y / z_safe
    with z_safe = z if z > z_min else z_min (default z_min=1e-3, focus=1.0).
    Documented as ANALYSIS, not game recovery.

CLI (PowerShell):
  & $env:MIMO_PYTHON tools\\python\\render_mesh_host.py \\
    --id 0x148,0x1cb --all-large 25 --out out/attr-render --size 512
"""
from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from render_poly_objects import (  # noqa: E402
    BASE,
    MAIN_PAIRS,
    POLY_PAIRS,
    TABLE,
    build,
    f32,
    u32,
)

# Fixed analysis light (not game lighting).
LIGHT = (0.3, 0.5, 1.0)
BASE_RGB = (92, 148, 210)
EDGE_RGB = (20, 28, 40)
BG_RGB = (18, 20, 28)
MAX_TRIS_DEFAULT = 12000
DEFAULT_VIEWS = (
    "fit-ortho",
    "front",
    "top",
    "side",
    "iso-xy",
    "iso-xz",
    "iso-yz",
)
EXTRA_VIEWS = ("tgp", "persp")

VIEW_INFO = {
    "fit-ortho": {
        "right": (1.0, 0.0, 0.0),
        "up": (0.0, 1.0, 0.0),
        "depth": (0.0, 0.0, 1.0),
        "label": "fit-ortho XY (legacy bbox)",
    },
    "front": {
        "right": (1.0, 0.0, 0.0),
        "up": (0.0, 1.0, 0.0),
        "depth": (0.0, 0.0, 1.0),
        "label": "front +Z",
    },
    "top": {
        "right": (1.0, 0.0, 0.0),
        "up": (0.0, 0.0, -1.0),
        "depth": (0.0, 1.0, 0.0),
        "label": "top +Y",
    },
    "side": {
        "right": (0.0, 0.0, 1.0),
        "up": (0.0, 1.0, 0.0),
        "depth": (1.0, 0.0, 0.0),
        "label": "side +X",
    },
}


def _norm(v):
    n = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    if n < 1e-12:
        return (0.0, 0.0, 0.0)
    return (v[0] / n, v[1] / n, v[2] / n)


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def _rot_xy(ax_deg: float, az_deg: float):
    """Rotate camera basis: Ry(az) * Rx(ax) applied to world right/up/depth."""
    ax = math.radians(ax_deg)
    az = math.radians(az_deg)
    # Rx then Ry on basis vectors expressed in world space.
    ca, sa = math.cos(ax), math.sin(ax)
    cz, sz = math.cos(az), math.sin(az)
    # Camera looks along +Z after rotations that tilt the named plane.
    # right = Ry(az)*Rx(ax)*(1,0,0)
    right = (cz, 0.0, -sz)
    # up0 = Rx(ax)*(0,1,0) = (0, ca, sa); then Ry(az)
    up = (sz * sa, ca, cz * sa)
    # depth0 = Rx(ax)*(0,0,1) = (0, -sa, ca); then Ry(az)
    depth = (sz * ca, -sa, cz * ca)
    return {
        "right": _norm(right),
        "up": _norm(up),
        "depth": _norm(depth),
        "label": f"iso ax={ax_deg} az={az_deg}",
    }


VIEW_INFO["iso-xy"] = _rot_xy(30.0, 45.0)
VIEW_INFO["iso-xz"] = _rot_xy(-35.0, 30.0)
VIEW_INFO["iso-yz"] = _rot_xy(20.0, -55.0)
VIEW_INFO["tgp"] = {
    "right": (1.0, 0.0, 0.0),
    "up": (0.0, 1.0, 0.0),
    "depth": (0.0, 0.0, 1.0),
    "label": "tgp-matrix*focus then fit-ortho",
}
VIEW_INFO["persp"] = {
    "right": (1.0, 0.0, 0.0),
    "up": (0.0, 1.0, 0.0),
    "depth": (0.0, 0.0, 1.0),
    "label": "persp host x=f*x/z_safe looking +Z",
}


def decode_object(
    poly: bytes,
    word_index: int,
    *,
    skip_words: int,
    max_links: int = 4096,
    max_tris: int = MAX_TRIS_DEFAULT,
):
    """Decode one object per tgp.c geometry_execute_object.

    skip_words=3 → geometry_mode&3 < 2 path; skip_words=0 → noskip path.
    Returns (tris, nonfinite_rejected, truncated).
    Each tri is ((x,y,z), (x,y,z), (x,y,z)).
    """
    tris: list[tuple] = []
    nonfinite_rejected = 0
    truncated = False
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
        return tris, nonfinite_rejected, truncated

    for _ in range(max_links):
        try:
            attr = rd()
        except EOFError:
            break
        if (attr & 3) == 0:
            break
        p3 = None
        try:
            for _k in range(skip_words):
                rd()
            p2 = (f32(rd()), f32(rd()), f32(rd()))
            if attr & 1:
                p3 = (f32(rd()), f32(rd()), f32(rd()))
        except EOFError:
            break

        pts = p0 + p1 + p2 + (p3 or ())
        if all(math.isfinite(c) for c in pts):
            tris.append((p0, p1, p2))
            if p3 is not None:
                tris.append((p0, p2, p3))
            if len(tris) >= max_tris:
                truncated = True
                break
        else:
            nonfinite_rejected += 1
            if len(tris) >= max_tris:
                truncated = True
                break

        # tgp.c geometry_execute_object p0/p1 update (exact cases).
        mode = (attr >> 8) & 3
        if mode in (0, 2):
            p0 = p2
            p1 = p3 if p3 is not None else p2
        elif mode == 1:
            p1 = p2
        else:  # mode == 3
            p0 = p3 if p3 is not None else p2
    return tris, nonfinite_rejected, truncated


def tgp_transform(point, matrix, focus_x: float, focus_y: float):
    """geometry_transform_point mirror (tgp.c). point=(x,y,z); w=1."""
    x, y, z = point[0], point[1], point[2]
    w = 1.0
    m = matrix
    rx = (m[0] * x + m[4] * y + m[8] * z + m[12] * w) * focus_x
    ry = (m[1] * x + m[5] * y + m[9] * z + m[13] * w) * focus_y
    rz = m[2] * x + m[6] * y + m[10] * z + m[14] * w
    rw = m[3] * x + m[7] * y + m[11] * z + m[15] * w
    return (rx, ry, rz), rw


def identity_matrix():
    return [
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0,
    ]


def apply_view_to_tris(tris, view: str, matrix, focus_x, focus_y, z_min, focus_p):
    """Return list of screen-space tris ((sx,sy,sz),...) plus world bounds.

    For orthographic views sx/sy are camera-basis projections (world units).
    For tgp, points are matrix-transformed first then projected with fit-ortho axes.
    For persp, sx/sy are already perspective-divided.
    """
    info = VIEW_INFO.get(view, VIEW_INFO["fit-ortho"])
    right, up, depth = info["right"], info["up"], info["depth"]
    projected = []
    xs, ys, zs = [], [], []

    def push_tri(p0, p1, p2):
        s0 = (_dot(p0, right), _dot(p0, up), _dot(p0, depth))
        s1 = (_dot(p1, right), _dot(p1, up), _dot(p1, depth))
        s2 = (_dot(p2, right), _dot(p2, up), _dot(p2, depth))
        projected.append((s0, s1, s2))
        for p in (p0, p1, p2):
            xs.append(p[0])
            ys.append(p[1])
            zs.append(p[2])

    if view == "persp":
        projected = []
        for tri in tris:
            spts = []
            ok = True
            for p in tri:
                z_safe = p[2] if p[2] > z_min else z_min
                if not math.isfinite(z_safe) or abs(z_safe) < 1e-12:
                    ok = False
                    break
                sx = focus_p * p[0] / z_safe
                sy = focus_p * p[1] / z_safe
                sz = z_safe
                spts.append((sx, sy, sz))
                xs.append(sx)
                ys.append(sy)
                zs.append(sz)
            if ok:
                projected.append(tuple(spts))
        bounds = (
            (min(xs), min(ys), min(zs)) if xs else (0.0, 0.0, 0.0),
            (max(xs), max(ys), max(zs)) if xs else (0.0, 0.0, 0.0),
        )
        return projected, bounds, info["label"]

    if view == "tgp":
        for tri in tris:
            pts = []
            for p in tri:
                tp, _w = tgp_transform(p, matrix, focus_x, focus_y)
                pts.append(tp)
            push_tri(*pts)
        bounds = (
            (min(xs), min(ys), min(zs)) if xs else (0.0, 0.0, 0.0),
            (max(xs), max(ys), max(zs)) if xs else (0.0, 0.0, 0.0),
        )
        return projected, bounds, info["label"]

    # Orthographic family (fit-ortho / front / top / side / iso-*)
    for tri in tris:
        push_tri(*tri)
    bounds = (
        (min(xs), min(ys), min(zs)) if xs else (0.0, 0.0, 0.0),
        (max(xs), max(ys), max(zs)) if xs else (0.0, 0.0, 0.0),
    )
    return projected, bounds, info["label"]


def bounds_of_tris(tris):
    if not tris:
        return None
    xs = [p[0] for t in tris for p in t]
    ys = [p[1] for t in tris for p in t]
    zs = [p[2] for t in tris for p in t]
    return {
        "min": [min(xs), min(ys), min(zs)],
        "max": [max(xs), max(ys), max(zs)],
    }


def rasterize(projected_tris, size: int, *, painter: bool = False):
    """Z-buffer raster + 1px wireframe overlay. Returns (rgb_bytes, coverage_px)."""
    if not projected_tris or size <= 0:
        buf = bytearray(BG_RGB * size * size) if size > 0 else bytearray()
        return buf, 0

    sxs = [p[0] for t in projected_tris for p in t]
    sys_ = [p[1] for t in projected_tris for p in t]
    minx, maxx = min(sxs), max(sxs)
    miny, maxy = min(sys_), max(sys_)
    spanx = max(maxx - minx, 1e-9)
    spany = max(maxy - miny, 1e-9)
    margin = 0.04
    usable = 1.0 - 2.0 * margin
    scale = min(usable * (size - 1) / spanx, usable * (size - 1) / spany)
    ox = (size - 1) * 0.5 - 0.5 * scale * (minx + maxx)
    oy = (size - 1) * 0.5 + 0.5 * scale * (miny + maxy)  # y flip

    def to_screen(p):
        sx = p[0] * scale + ox
        sy = oy - p[1] * scale
        return sx, sy, p[2]

    light = _norm(LIGHT)
    n_px = size * size
    color_buf = bytearray(BG_RGB * n_px)
    zbuf = [1e30] * n_px
    coverage = 0

    screen_tris = []
    for tri in projected_tris:
        s0, s1, s2 = to_screen(tri[0]), to_screen(tri[1]), to_screen(tri[2])
        screen_tris.append((s0, s1, s2))

    order = list(range(len(screen_tris)))
    if painter:
        order.sort(key=lambda i: -sum(p[2] for p in screen_tris[i]))

    for ti in order:
        (x0, y0, z0), (x1, y1, z1), (x2, y2, z2) = screen_tris[ti]
        area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        if area == 0.0:
            continue
        # Face normal from original world/projected 3D (before screen y-flip).
        p0, p1, p2 = projected_tris[ti]
        e1 = (p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2])
        e2 = (p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2])
        n = _cross(e1, e2)
        nn = _norm(n)
        ndotl = abs(_dot(nn, light))
        shade = 0.28 + 0.72 * ndotl
        cr = int(max(0, min(255, BASE_RGB[0] * shade)))
        cg = int(max(0, min(255, BASE_RGB[1] * shade)))
        cb = int(max(0, min(255, BASE_RGB[2] * shade)))
        packed = (cr << 16) | (cg << 8) | cb

        minx_i = max(0, int(math.floor(min(x0, x1, x2))))
        maxx_i = min(size - 1, int(math.ceil(max(x0, x1, x2))))
        miny_i = max(0, int(math.floor(min(y0, y1, y2))))
        maxy_i = min(size - 1, int(math.ceil(max(y0, y1, y2))))
        if minx_i > maxx_i or miny_i > maxy_i:
            continue

        for py in range(miny_i, maxy_i + 1):
            fy = py + 0.5
            for px in range(minx_i, maxx_i + 1):
                fx = px + 0.5
                w0 = (x1 - x0) * (fy - y0) - (y1 - y0) * (fx - x0)
                w1 = (x2 - x1) * (fy - y1) - (y2 - y1) * (fx - x1)
                w2 = (x0 - x2) * (fy - y2) - (y0 - y2) * (fx - x2)
                if area > 0:
                    if w0 < 0 or w1 < 0 or w2 < 0:
                        continue
                else:
                    if w0 > 0 or w1 > 0 or w2 > 0:
                        continue
                inv = 1.0 / area
                b0 = w1 * inv
                b1 = w2 * inv
                b2 = w0 * inv
                z = b0 * z0 + b1 * z1 + b2 * z2
                idx = py * size + px
                if z < zbuf[idx]:
                    zbuf[idx] = z
                    o = idx * 3
                    color_buf[o] = (packed >> 16) & 255
                    color_buf[o + 1] = (packed >> 8) & 255
                    color_buf[o + 2] = packed & 255
                    coverage += 1

    # 1px wireframe overlay (analysis legibility; not a game outline).
    def plot_edge(ax, ay, bx, by):
        steps = int(max(abs(bx - ax), abs(by - ay))) + 1
        for s in range(steps + 1):
            t = s / steps
            x = int(round(ax + (bx - ax) * t))
            y = int(round(ay + (by - ay) * t))
            if 0 <= x < size and 0 <= y < size:
                o = (y * size + x) * 3
                color_buf[o] = EDGE_RGB[0]
                color_buf[o + 1] = EDGE_RGB[1]
                color_buf[o + 2] = EDGE_RGB[2]

    for s0, s1, s2 in screen_tris:
        plot_edge(s0[0], s0[1], s1[0], s1[1])
        plot_edge(s1[0], s1[1], s2[0], s2[1])
        plot_edge(s2[0], s2[1], s0[0], s0[1])

    return bytes(color_buf), coverage


def try_import_pil():
    try:
        from PIL import Image, ImageDraw, ImageFont  # type: ignore

        return Image, ImageDraw, ImageFont
    except Exception:
        return None, None, None


def write_image(path: Path, size: int, rgb: bytes) -> str:
    """Write PNG via Pillow if available, else PPM. Returns written path str."""
    Image, _Draw, _Font = try_import_pil()
    if Image is not None and rgb:
        img = Image.frombytes("RGB", (size, size), rgb)
        path = path.with_suffix(".png")
        img.save(path)
        return str(path)
    path = path.with_suffix(".ppm")
    raw = bytearray()
    # rgb already tightly packed RGB
    path.write_bytes(f"P6\n{size} {size}\n255\n".encode() + rgb)
    return str(path)


def draw_contact_sheet(
    path: Path,
    rows,
    cell: int,
    col_labels,
    Image,
    ImageDraw,
    ImageFont,
) -> str:
    """rows: list of list of RGB bytes (or None). One row per object id."""
    if not rows or Image is None:
        return ""
    n_cols = max(len(r) for r in rows)
    n_rows = len(rows)
    label_w = 140
    header_h = 36
    pad = 4
    w = label_w + n_cols * (cell + pad) + pad
    h = header_h + n_rows * (cell + pad) + pad
    sheet = Image.new("RGB", (w, h), (12, 14, 20))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.load_default()
    except Exception:
        font = None

    for ci, lab in enumerate(col_labels[:n_cols]):
        x = label_w + ci * (cell + pad) + 2
        draw.text((x, 8), lab, fill=(180, 190, 210), font=font)

    for ri, (meta, cells) in enumerate(rows):
        y0 = header_h + ri * (cell + pad)
        draw.text((6, y0 + cell // 2 - 6), meta["label"], fill=(200, 210, 230), font=font)
        for ci, rgb in enumerate(cells):
            if rgb is None:
                continue
            x0 = label_w + ci * (cell + pad)
            img = Image.frombytes("RGB", (cell, cell), rgb)
            sheet.paste(img, (x0, y0))

    out = path.with_suffix(".png")
    sheet.save(out)
    return str(out)


def parse_id_list(text: str) -> list[int]:
    ids = []
    for part in text.replace(" ", "").split(","):
        if not part:
            continue
        ids.append(int(part, 0))
    return ids


def parse_matrix_args(args) -> tuple[list[float], float, float, str]:
    if args.matrix:
        vals = [float(x) for x in args.matrix]
        if len(vals) != 16:
            raise SystemExit("--matrix requires exactly 16 floats")
        return vals, args.focus_x, args.focus_y, "cli-matrix"
    if args.transforms:
        data = json.loads(Path(args.transforms).read_text(encoding="utf-8"))
        mat = data.get("matrix") or data.get("values")
        if mat is None or len(mat) != 16:
            raise SystemExit("--transforms JSON must contain matrix/values with 16 floats")
        fx = float(data.get("focus_x", args.focus_x))
        fy = float(data.get("focus_y", args.focus_y))
        return [float(x) for x in mat], fx, fy, f"transforms:{args.transforms}"
    return identity_matrix(), 1.0, 1.0, "identity-fallback"


def lookup_table(main_img: bytes, obj_id: int):
    off = (TABLE - BASE) + obj_id * 16
    if off < 0 or off + 16 > len(main_img):
        return None
    w0, w1, w2, w3 = struct.unpack_from("<IIII", main_img, off)
    word_index = w2 & 0x7FFFFF
    return {
        "id": obj_id,
        "w0": w0,
        "w1": w1,
        "w2": w2,
        "w3": w3,
        "word_index": word_index,
        "poly_byte": word_index * 4,
        "poly_rom": bool(w2 & 0x00800000),
        "table_offset": off,
    }


def scan_all_large(main_img: bytes, poly: bytes, limit: int, scan_max: int, max_tris: int):
    ranked = []
    for obj_id in range(scan_max + 1):
        rec = lookup_table(main_img, obj_id)
        if rec is None:
            break
        if rec["w2"] == 0 or rec["word_index"] == 0:
            continue
        if rec["poly_byte"] + 24 > len(poly):
            continue
        tris, _nf, _tr = decode_object(
            poly, rec["word_index"], skip_words=3, max_tris=max_tris
        )
        if tris:
            ranked.append((len(tris), obj_id, rec))
    ranked.sort(key=lambda t: (-t[0], t[1]))
    return ranked[:limit]


def score_view(coverage: int, n_tris: int, label: str, decode: str) -> float:
    """Rank candidates. coverage is PIXEL count (not a 0..1 fill ratio)."""
    if n_tris <= 0 or coverage <= 0:
        return -1.0
    # Log pixel coverage so huge flat fills do not dominate by /n_tris.
    cov_term = math.log2(1.0 + coverage)
    mass = math.log2(1.0 + n_tris)
    dec_bonus = 20.0 if decode == "skip3" else 0.0
    bonus = 0.0
    if "iso" in label:
        bonus = 0.3
    if label.startswith("front"):
        bonus = 0.15
    if label.startswith("top") or label.startswith("side"):
        bonus = 0.2
    # Prefer denser meshes and solid coverage; skip3 is the coherent poly-ROM path.
    return dec_bonus + mass * 3.0 + cov_term + bonus


def main() -> int:
    ap = argparse.ArgumentParser(description="Host mesh rasterizer for measured poly-ROM objects")
    ap.add_argument("--id", default="", help="comma-separated object ids (hex/dec), e.g. 0x148,0x1cb")
    ap.add_argument("--all-large", type=int, default=0, metavar="N",
                    help="also render top N ids by skip3 triangle count")
    ap.add_argument("--scan-max", type=lambda s: int(s, 0), default=0x2000,
                    help="max object id scanned by --all-large (default 0x2000)")
    ap.add_argument("--out", default="out/attr-render")
    ap.add_argument("--size", type=int, default=512)
    ap.add_argument("--cell", type=int, default=96, help="contact-sheet cell size")
    ap.add_argument("--views", default=",".join(DEFAULT_VIEWS),
                    help=f"comma views; known: {','.join(list(VIEW_INFO))}")
    ap.add_argument("--decode", default="both", choices=("skip3", "noskip", "both"))
    ap.add_argument("--matrix", nargs=16, type=float, metavar="M",
                    help="16 floats for --view tgp")
    ap.add_argument("--transforms", default="", help="JSON with matrix/values + focus_x/y")
    ap.add_argument("--focus-x", type=float, default=1.0)
    ap.add_argument("--focus-y", type=float, default=1.0)
    ap.add_argument("--persp-focus", type=float, default=1.0)
    ap.add_argument("--persp-zmin", type=float, default=1e-3)
    ap.add_argument("--painter", action="store_true", help="painter sort before z-buffer")
    ap.add_argument("--max-tris", type=int, default=MAX_TRIS_DEFAULT)
    ap.add_argument("--rom-dir", default="roms/vf2")
    ap.add_argument("--no-contact", action="store_true")
    ap.add_argument("--no-full", action="store_true", help="skip full-size per-id PNGs")
    args = ap.parse_args()

    rom_dir = Path(args.rom_dir)
    if not rom_dir.is_dir():
        print(f"ERROR: ROM dir missing: {rom_dir}", file=sys.stderr)
        return 2
    # render_poly_objects.ROM_DIR is module-level; rebind for build().
    import render_poly_objects as rpo

    rpo.ROM_DIR = rom_dir

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    views = [v.strip() for v in args.views.split(",") if v.strip()]
    unknown = [v for v in views if v not in VIEW_INFO]
    if unknown:
        print(f"ERROR: unknown views {unknown}; known={list(VIEW_INFO)}", file=sys.stderr)
        return 2

    decodes = []
    if args.decode in ("skip3", "both"):
        decodes.append(("skip3", 3))
    if args.decode in ("noskip", "both"):
        decodes.append(("noskip", 0))

    matrix, focus_x, focus_y, matrix_label = parse_matrix_args(args)
    tgp_label = (
        VIEW_INFO["tgp"]["label"]
        if matrix_label != "identity-fallback"
        else "tgp IDENTITY FALLBACK (no matrix/transforms) fit-ortho"
    )

    print(f"building ROM images from {rom_dir} ...")
    main_img = build(MAIN_PAIRS, 0x02400000)
    poly = build(POLY_PAIRS, 0x02000000)
    print(f"main={len(main_img):#x} poly={len(poly):#x}")

    id_list: list[int] = []
    if args.id:
        id_list.extend(parse_id_list(args.id))

    all_large_rows = []
    if args.all_large > 0:
        print(f"scanning object table 0..{args.scan_max:#x} for top {args.all_large} by skip3 tris ...")
        ranked = scan_all_large(main_img, poly, args.all_large, args.scan_max, args.max_tris)
        for n_tris, obj_id, rec in ranked:
            all_large_rows.append(
                {
                    "id": f"0x{obj_id:03x}",
                    "tris_skip3": n_tris,
                    "w2": f"{rec['w2']:#010x}",
                    "word_index": f"{rec['word_index']:#x}",
                }
            )
            if obj_id not in id_list:
                id_list.append(obj_id)
        print("all-large top:")
        for row in all_large_rows:
            print(f"  {row['id']} tris_skip3={row['tris_skip3']} word={row['word_index']}")

    if not id_list:
        print("ERROR: provide --id and/or --all-large", file=sys.stderr)
        return 2

    Image, ImageDraw, ImageFont = try_import_pil()
    use_png = Image is not None
    print(f"PIL={'yes' if use_png else 'no — will write PPM'}")

    col_labels = []
    col_keys = []
    for dec_name, _sw in decodes:
        for view in views:
            col_labels.append(f"{dec_name}|{view}")
            col_keys.append((dec_name, view))

    report_objects = []
    sheet_rows = []

    for obj_id in id_list:
        rec = lookup_table(main_img, obj_id)
        label = f"0x{obj_id:03x}"
        if rec is None:
            print(f"{label}: OUT OF TABLE")
            report_objects.append({"id": label, "error": "out_of_table"})
            sheet_rows.append(({"label": f"{label} OOB"}, [None] * len(col_keys)))
            continue
        if rec["poly_byte"] + 24 > len(poly) or rec["word_index"] == 0:
            print(f"{label}: no usable poly pointer w2={rec['w2']:#010x}")
            report_objects.append(
                {
                    "id": label,
                    "w0": f"{rec['w0']:#010x}",
                    "w2": f"{rec['w2']:#010x}",
                    "w3": f"{rec['w3']:#010x}",
                    "word_index": f"{rec['word_index']:#x}",
                    "poly_byte": f"{rec['poly_byte']:#x}",
                    "poly_rom": rec["poly_rom"],
                    "error": "no_poly_pointer",
                }
            )
            sheet_rows.append(({"label": f"{label} empty"}, [None] * len(col_keys)))
            continue

        decoded = {}
        files = []
        best = {"view": None, "decode": None, "score": -1.0, "file": ""}
        entry = {
            "id": label,
            "w0": f"{rec['w0']:#010x}",
            "w1": f"{rec['w1']:#010x}",
            "w2": f"{rec['w2']:#010x}",
            "w3": f"{rec['w3']:#010x}",
            "word_index": f"{rec['word_index']:#x}",
            "poly_byte": f"{rec['poly_byte']:#x}",
            "poly_rom": rec["poly_rom"],
            "table_offset": f"{rec['table_offset']:#x}",
            "tris_skip3": 0,
            "tris_noskip": 0,
            "nonfinite_rejected_skip3": 0,
            "nonfinite_rejected_noskip": 0,
            "truncated": {},
            "bounds": {},
            "coverage": {},
            "tgp_matrix_label": matrix_label if "tgp" in views else None,
            "files": files,
            "best_view": None,
            "best_file": None,
        }

        cell_rgbs = []
        full_cache = {}  # (dec, view) -> (rgb, size) for best-size render

        for dec_name, skip_words in decodes:
            tris, nf, trunc = decode_object(
                poly, rec["word_index"], skip_words=skip_words, max_tris=args.max_tris
            )
            decoded[dec_name] = tris
            key_n = f"tris_{dec_name}"
            entry[key_n] = len(tris)
            entry[f"nonfinite_rejected_{dec_name}"] = nf
            entry["truncated"][dec_name] = bool(trunc)
            b = bounds_of_tris(tris)
            if b:
                entry["bounds"][dec_name] = b
            print(
                f"{label} {dec_name}: tris={len(tris)} nonfinite={nf} "
                f"word={rec['word_index']:#x} byte={rec['poly_byte']:#x} "
                f"w0={rec['w0']:#010x} w2={rec['w2']:#010x} w3={rec['w3']:#010x}"
            )

        for view in views:
            for dec_name, _sw in decodes:
                tris = decoded.get(dec_name) or []
                proj, _b, label_v = apply_view_to_tris(
                    tris,
                    view,
                    matrix,
                    focus_x,
                    focus_y,
                    args.persp_zmin,
                    args.persp_focus,
                )
                if view == "tgp":
                    label_v = tgp_label
                rgb_cell, cov = rasterize(proj, args.cell, painter=args.painter)
                cell_rgbs.append(rgb_cell if tris else None)
                entry["coverage"][f"{dec_name}|{view}"] = cov

                if not args.no_full and tris:
                    rgb_full, cov_full = rasterize(proj, args.size, painter=args.painter)
                    full_cache[(dec_name, view)] = (rgb_full, cov_full)
                    sc = score_view(cov_full, len(tris), label_v, dec_name)
                    if sc > best["score"]:
                        best["score"] = sc
                        best["view"] = view
                        best["decode"] = dec_name
                elif not tris:
                    cell_rgbs[-1] = None

        # Write full-size PNGs for every view/decode that produced tris,
        # plus ensure best-view path is recorded.
        if not args.no_full:
            for key, (rgb_full, _cov) in full_cache.items():
                dec_name, view = key
                fname = out_dir / f"id_{obj_id:03x}_{dec_name}_{view}_{args.size}.png"
                written = write_image(fname, args.size, rgb_full)
                if written not in files:
                    files.append(written)
                if best["view"] == view and best["decode"] == dec_name:
                    best["file"] = written
            if not best["file"] and files:
                best["file"] = files[0]

        entry["best_view"] = best["view"]
        entry["best_decode"] = best["decode"]
        entry["best_file"] = best["file"] or (files[0] if files else None)
        report_objects.append(entry)
        meta_label = (
            f"{label} s={entry['tris_skip3']} n={entry['tris_noskip']}"
        )
        sheet_rows.append(({"label": meta_label}, cell_rgbs))

    sheet_path = ""
    if not args.no_contact and use_png:
        sheet_path = draw_contact_sheet(
            out_dir / "contact_sheet.png",
            sheet_rows,
            args.cell,
            col_labels,
            Image,
            ImageDraw,
            ImageFont,
        )
        print(f"contact sheet: {sheet_path}")

    report = {
        "tool": "render_mesh_host",
        "phase": "A",
        "disclaimer": (
            "Host-side analysis of measured polygon-ROM object meshes. "
            "NOT recovered C, NOT a logo/title witness, NOT game-accurate raster."
        ),
        "geometry_source": "src/hardware/tgp.c geometry_execute_object / geometry_transform_point",
        "decode_note": (
            "skip3 = geometry_mode&3 < 2 (skip 3 words after attr); "
            "noskip = skip 0 words. Mode 0/2 p0/p1 update follows tgp.c "
            "(p1 = p3 if quad else p2)."
        ),
        "projection_note": (
            "Views are host analysis cameras. tgp applies tgp.c matrix+focus if "
            "provided else identity fallback. persp is x=f*x/z_safe looking +Z."
        ),
        "rom_dir": str(rom_dir),
        "out": str(out_dir),
        "size": args.size,
        "cell": args.cell,
        "views": views,
        "decodes": [d for d, _ in decodes],
        "tgp_matrix_label": matrix_label,
        "contact_sheet": sheet_path,
        "all_large": all_large_rows,
        "objects": report_objects,
    }
    report_path = out_dir / "render_report.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"report: {report_path}")
    print(f"objects rendered: {len(report_objects)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
