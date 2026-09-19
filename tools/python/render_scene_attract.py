#!/usr/bin/env python3
"""Multi-object attract phase5 scene reconstruction from measured submits.

ANALYSIS TOOL — host assembly of measured object-table meshes + timeline.
NOT recovered game C. NOT a CRT/framebuffer dump. NOT a named logo/title
witness. Fail-closed: composite layouts without measured transforms are
labeled host_assembly.

Evidence used:
  - object table 0x020e0004[id*16]; poly word_index = w2 & 0x7fffff
  - timeline from build_phase5_timeline.py (table reads + geo/FIFO w0 submits)
  - FIFO protocol tags (0x14802929 family) as optional analysis fills

Decode imported from tools/python/scene_decode_helper.py (stable surface).
Prefers render_mesh_host.decode_object when present (P1 may improve it);
falls back to local tgp.c skip3/noskip float-link decode.

Shared view: fixed analysis camera (default iso-xz), SAME scale across all
windows (global projected bounds or first-frame bounds).

Color policy:
  - default: deterministic per-id palette (analysis legibility)
  - --fifo-fill: if a nearby FIFO protocol tag matches YY=ZZ pattern, use
    extracted bytes as analysis fill; label protocol_tag, NOT proven RGB

CLI:
  $MIMO_PYTHON tools/python/render_scene_attract.py \
    --timeline out/attr-render/scene/phase5_timeline.jsonl \
    --out out/attr-render/scene \
    --decode skip3 --view iso-xz --window 20 --size 384

  $MIMO_PYTHON tools/python/render_scene_attract.py --composite-only ...
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any, Iterable

sys.path.insert(0, str(Path(__file__).resolve().parent))
from scene_decode_helper import (  # noqa: E402
    DEFAULT_ROM_DIR,
    import_decode_object,
    index_palette,
    load_object_table,
    load_poly,
    protocol_tag_rgb,
    try_import_pil,
)
from render_poly_objects import MAIN_PAIRS, POLY_PAIRS, build  # noqa: E402
import render_poly_objects as rpo  # noqa: E402

# Match render_mesh_host analysis cameras (iso-xz default for scenes).
try:
    from render_mesh_host import VIEW_INFO as _RMH_VIEW_INFO  # noqa: E402
    from render_mesh_host import apply_view_to_tris as _rmh_apply_view
    from render_mesh_host import bounds_of_tris  # noqa: E402
    from render_mesh_host import identity_matrix  # noqa: E402

    VIEW_INFO = _RMH_VIEW_INFO

    def apply_view_to_tris(tris, view, matrix=None, focus_x=1.0, focus_y=1.0,
                           z_min=1e-3, focus_p=1.0):
        # Stable scene-facing wrapper: identity + focus 1.0 when transforms
        # are absent (fail-closed; not a recovered camera).
        return _rmh_apply_view(
            tris,
            view,
            matrix if matrix is not None else identity_matrix(),
            focus_x,
            focus_y,
            z_min,
            focus_p,
        )

except Exception:
    _ISO_XZ_RIGHT = (0.866, 0.0, -0.5)
    _ISO_XZ_UP = (0.354, 0.866, 0.354)
    _ISO_XZ_DEPTH = (0.354, -0.5, 0.707)
    VIEW_INFO = {
        "iso-xz": {
            "right": _ISO_XZ_RIGHT,
            "up": _ISO_XZ_UP,
            "depth": _ISO_XZ_DEPTH,
            "label": "iso-xz host fallback",
        },
        "fit-ortho": {
            "right": (1.0, 0.0, 0.0),
            "up": (0.0, 1.0, 0.0),
            "depth": (0.0, 0.0, 1.0),
            "label": "fit-ortho",
        },
        "top": {
            "right": (1.0, 0.0, 0.0),
            "up": (0.0, 0.0, -1.0),
            "depth": (0.0, 1.0, 0.0),
            "label": "top +Y",
        },
    }

    def apply_view_to_tris(tris, view, matrix=None, focus_x=1.0, focus_y=1.0,
                           z_min=1e-3, focus_p=1.0):
        info = VIEW_INFO.get(view, VIEW_INFO["fit-ortho"])
        right, up, depth = info["right"], info["up"], info["depth"]
        projected = []
        xs, ys, zs = [], [], []
        for tri in tris:
            spts = []
            for p in tri:
                s = (
                    p[0] * right[0] + p[1] * right[1] + p[2] * right[2],
                    p[0] * up[0] + p[1] * up[1] + p[2] * up[2],
                    p[0] * depth[0] + p[1] * depth[1] + p[2] * depth[2],
                )
                spts.append(s)
                xs.append(p[0]); ys.append(p[1]); zs.append(p[2])
            projected.append(tuple(spts))
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
        return {"min": [min(xs), min(ys), min(zs)], "max": [max(xs), max(ys), max(zs)]}


BG_RGB = (16, 18, 24)
EDGE_RGB = (18, 22, 30)
LIGHT = (0.3, 0.5, 1.0)
# Scene sanity window: host analysis only. ROM decode can emit finite but
# astronomical floats when skip3/noskip does not match that object's mode.
SCENE_MAX_ABS = 1.0e4
SCENE_MIN_EXTENT = 1.0e-6


def _norm(v):
    n = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    if n < 1e-12:
        return (0.0, 0.0, 0.0)
    return (v[0] / n, v[1] / n, v[2] / n)


def _cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def load_timeline(path: Path) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    events: list[dict[str, Any]] = []
    summary: dict[str, Any] = {}
    with path.open("r", errors="replace") as fh:
        for line in fh:
            if not line.startswith("{"):
                continue
            try:
                rec = json.loads(line)
            except json.JSONDecodeError:
                continue
            t = rec.get("type")
            if t == "object_event":
                events.append(rec)
            elif t == "summary":
                summary = rec
    events.sort(key=lambda e: (int(e.get("step") or 0), e.get("kind") or ""))
    return events, summary


def group_windows(
    events: list[dict[str, Any]],
    *,
    mode: str,
    window: int,
    step_bucket: int,
) -> list[dict[str, Any]]:
    """Group object events into temporal windows.

    modes:
      event — fixed number of object events per window (default 20)
      step  — fixed step bucket size
    """
    windows: list[dict[str, Any]] = []
    if not events:
        return windows
    if mode == "step":
        buckets: dict[int, list[dict[str, Any]]] = defaultdict(list)
        for ev in events:
            step = int(ev.get("step") or 0)
            b = step // max(1, step_bucket)
            buckets[b].append(ev)
        for b in sorted(buckets):
            evs = buckets[b]
            windows.append(_window_from_events(evs, index=len(windows)))
        return windows

    # event mode
    win = max(1, window)
    for i in range(0, len(events), win):
        chunk = events[i : i + win]
        windows.append(_window_from_events(chunk, index=len(windows)))
    return windows


def _window_from_events(evs: list[dict[str, Any]], *, index: int) -> dict[str, Any]:
    ids: list[int] = []
    seen: set[int] = set()
    tags: Counter[int] = Counter()
    kind_c: Counter[str] = Counter()
    steps = []
    for ev in evs:
        steps.append(int(ev.get("step") or 0))
        kind_c[ev.get("kind") or "?"] += 1
        iid = ev.get("id_int")
        if iid is None and ev.get("id"):
            iid = int(str(ev["id"]), 16)
        if iid is not None and iid not in seen:
            seen.add(int(iid))
            ids.append(int(iid))
        for nt in ev.get("near_tags") or []:
            try:
                tags[int(str(nt.get("w0") or "0"), 16)] += 1
            except ValueError:
                continue
    return {
        "index": index,
        "event_count": len(evs),
        "ids": ids,
        "step_min": min(steps) if steps else None,
        "step_max": max(steps) if steps else None,
        "kind_counts": dict(kind_c),
        "tag_counts": tags,
        "events": evs,
    }


def offset_tris(tris, dx: float, dy: float, dz: float):
    out = []
    for t in tris:
        out.append(tuple((p[0] + dx, p[1] + dy, p[2] + dz) for p in t))
    return out


def mesh_max_abs(tris) -> float:
    if not tris:
        return 0.0
    m = 0.0
    for t in tris:
        for p in t:
            m = max(m, abs(p[0]), abs(p[1]), abs(p[2]))
    return m


def sanitize_tris(tris, max_abs: float = SCENE_MAX_ABS):
    """Drop triangles with extreme/non-finite coords. Fail-closed for scene."""
    out = []
    for t in tris:
        pts = []
        ok = True
        for p in t:
            if not all(math.isfinite(c) and abs(c) <= max_abs for c in p):
                ok = False
                break
            pts.append(p)
        if ok and len(pts) == 3:
            out.append(tuple(pts))
    return out


def center_tris(tris):
    """Host analysis: translate mesh so AABB center is at origin."""
    if not tris:
        return tris
    xs = [p[0] for t in tris for p in t]
    ys = [p[1] for t in tris for p in t]
    zs = [p[2] for t in tris for p in t]
    cx = 0.5 * (min(xs) + max(xs))
    cy = 0.5 * (min(ys) + max(ys))
    cz = 0.5 * (min(zs) + max(zs))
    return offset_tris(tris, -cx, -cy, -cz)


def extent_of_tris(tris) -> float:
    if not tris:
        return 1.0
    xs = [p[0] for t in tris for p in t]
    ys = [p[1] for t in tris for p in t]
    zs = [p[2] for t in tris for p in t]
    return max(max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs), SCENE_MIN_EXTENT)


def grid_offsets(n: int, spacing: float) -> list[tuple[float, float, float]]:
    if n <= 0:
        return []
    if n == 1:
        return [(0.0, 0.0, 0.0)]
    cols = max(1, int(math.ceil(math.sqrt(n))))
    rows = int(math.ceil(n / cols))
    out = []
    for i in range(n):
        r, c = divmod(i, cols)
        ox = (c - (cols - 1) * 0.5) * spacing
        oy = 0.0
        oz = (r - (rows - 1) * 0.5) * spacing
        out.append((ox, oy, oz))
    return out


def choose_fill(
    obj_id: int,
    palette: dict[int, tuple[int, int, int]],
    tag_counts: Counter[int],
    *,
    fifo_fill: bool,
) -> tuple[tuple[int, int, int], str]:
    base = palette.get(obj_id, (120, 140, 170))
    if not fifo_fill or not tag_counts:
        return base, "palette"
    # Dominant nearby protocol tag → analysis fill (not proven RGB).
    best_val, best_n = tag_counts.most_common(1)[0]
    rgb = protocol_tag_rgb(best_val)
    if rgb is None:
        return base, "palette"
    # Blend slightly with palette so multi-object identity remains visible.
    mixed = (
        (rgb[0] + base[0]) // 2,
        (rgb[1] + base[1]) // 2,
        (rgb[2] + base[2]) // 2,
    )
    return mixed, f"protocol_tag:{best_val:#010x}"


def project_bounds_all(
    meshes: dict[int, list],
    view: str,
    *,
    max_abs: float = SCENE_MAX_ABS,
) -> tuple[float, float, float, float] | None:
    """Global projected (sx,sy) bounds for stable scale (sanitized meshes only)."""
    minx = miny = math.inf
    maxx = maxy = -math.inf
    used = 0
    for tris in meshes.values():
        clean = sanitize_tris(tris, max_abs=max_abs)
        if not clean:
            continue
        proj, _b, _lab = apply_view_to_tris(clean, view)
        for tri in proj:
            for p in tri:
                if not math.isfinite(p[0]) or not math.isfinite(p[1]):
                    continue
                minx = min(minx, p[0]); maxx = max(maxx, p[0])
                miny = min(miny, p[1]); maxy = max(maxy, p[1])
        used += 1
    if used == 0 or not math.isfinite(minx):
        return None
    # Guard against a single residual outlier collapsing the scene.
    if (maxx - minx) > max_abs or (maxy - miny) > max_abs:
        return None
    return minx, maxx, miny, maxy


def rasterize_scene(
    colored_tris: list[tuple[tuple, tuple, tuple, tuple[int, int, int], str]],
    size: int,
    *,
    scale_bounds: tuple[float, float, float, float] | None,
    painter: bool = True,
    draw_edges: bool = False,
) -> tuple[bytes, int, dict[str, Any]]:
    """Z-buffer raster with fixed projected scale.

    colored_tris: list of (p0, p1, p2, rgb, label) already in projected space.
    scale_bounds: (minx, maxx, miny, maxy) in projected units; None → auto-fit.
    """
    Image, _Draw, _Font = try_import_pil()
    if size <= 0:
        return b"", 0, {}
    if not colored_tris:
        buf = bytearray(BG_RGB * size * size)
        return bytes(buf), 0, {"empty": True}

    if scale_bounds is None:
        sxs = [p[0] for t in colored_tris for p in t[:3]]
        sys_ = [p[1] for t in colored_tris for p in t[:3]]
        minx, maxx = min(sxs), max(sxs)
        miny, maxy = min(sys_), max(sys_)
    else:
        minx, maxx, miny, maxy = scale_bounds

    spanx = max(maxx - minx, 1e-9)
    spany = max(maxy - miny, 1e-9)
    margin = 0.05
    usable = 1.0 - 2.0 * margin
    scale = min(usable * (size - 1) / spanx, usable * (size - 1) / spany)
    ox = (size - 1) * 0.5 - 0.5 * scale * (minx + maxx)
    oy = (size - 1) * 0.5 + 0.5 * scale * (miny + maxy)

    def to_screen(p):
        return (p[0] * scale + ox, oy - p[1] * scale, p[2])

    light = _norm(LIGHT)
    n_px = size * size
    color_buf = bytearray(BG_RGB * n_px)
    zbuf = [1e30] * n_px
    coverage = 0

    screen = []
    for tri in colored_tris:
        p0, p1, p2 = tri[0], tri[1], tri[2]
        rgb = tri[3]
        screen.append((to_screen(p0), to_screen(p1), to_screen(p2), rgb, tri[4] if len(tri) > 4 else ""))

    order = list(range(len(screen)))
    if painter:
        order.sort(key=lambda i: -sum(p[2] for p in screen[i][:3]))

    for ti in order:
        (x0, y0, z0), (x1, y1, z1), (x2, y2, z2), rgb, _lab = screen[ti]
        area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        if area == 0.0:
            continue
        # Shade with host light (not game lighting).
        p0, p1, p2 = colored_tris[ti][0], colored_tris[ti][1], colored_tris[ti][2]
        e1 = (p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2])
        e2 = (p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2])
        nn = _norm(_cross(e1, e2))
        shade = 0.30 + 0.70 * abs(_dot(nn, light))
        cr = int(max(0, min(255, rgb[0] * shade)))
        cg = int(max(0, min(255, rgb[1] * shade)))
        cb = int(max(0, min(255, rgb[2] * shade)))
        packed_r, packed_g, packed_b = cr, cg, cb

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
                    color_buf[o] = packed_r
                    color_buf[o + 1] = packed_g
                    color_buf[o + 2] = packed_b
                    coverage += 1

    meta = {
        "scale_bounds": [minx, maxx, miny, maxy],
        "coverage_px": coverage,
        "tri_count": len(colored_tris),
        "size": size,
    }
    return bytes(color_buf), coverage, meta


def write_image(path: Path, size_w: int, size_h: int, rgb: bytes) -> str:
    Image, _Draw, _Font = try_import_pil()
    if Image is not None and rgb:
        img = Image.frombytes("RGB", (size_w, size_h), rgb)
        path = path.with_suffix(".png")
        img.save(path)
        return str(path)
    path = path.with_suffix(".ppm")
    raw = bytearray()
    for c in rgb:
        raw.append(c)
    path.write_bytes(f"P6\n{size_w} {size_h}\n255\n".encode() + rgb)
    return str(path)


def draw_strip(
    path: Path,
    cells: list[tuple[str, bytes, int]],
    *,
    cell: int,
) -> str:
    """Contact strip: horizontal PNG with labels under each window cell."""
    Image, ImageDraw, ImageFont = try_import_pil()
    if Image is None or not cells:
        return ""
    pad = 6
    label_h = 22
    w = pad + len(cells) * (cell + pad)
    h = pad + cell + label_h + pad
    sheet = Image.new("RGB", (w, h), (12, 14, 20))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.load_default()
    except Exception:
        font = None
    for i, (label, rgb, size) in enumerate(cells):
        x0 = pad + i * (cell + pad)
        y0 = pad
        if rgb and size > 0:
            img = Image.frombytes("RGB", (size, size), rgb)
            if size != cell:
                img = img.resize((cell, cell))
            sheet.paste(img, (x0, y0))
        draw.text((x0 + 2, y0 + cell + 4), label[:48], fill=(190, 200, 220), font=font)
    out = path.with_suffix(".png")
    sheet.save(out)
    return str(out)


def decode_objects(
    ids: Iterable[int],
    by_id: dict[int, dict[str, Any]],
    poly: bytes,
    decode_fn,
    *,
    skip_words: int,
    max_tris: int,
    auto_mode_fallback: bool = True,
    max_abs: float = SCENE_MAX_ABS,
) -> tuple[dict[int, list], dict[int, dict[str, Any]]]:
    """Decode objects; optional per-object skip3/noskip fallback for extremes."""
    meshes: dict[int, list] = {}
    stats: dict[int, dict[str, Any]] = {}
    modes = [skip_words]
    if auto_mode_fallback:
        for alt in (3, 0):
            if alt not in modes:
                modes.append(alt)

    def _decode(sw: int):
        try:
            return decode_fn(poly, rec["word_index"], skip_words=sw, max_tris=max_tris)
        except TypeError:
            return decode_fn(poly, rec["word_index"], sw)

    for obj_id in ids:
        rec = by_id.get(obj_id)
        if not rec or rec.get("word_index", 0) == 0:
            stats[obj_id] = {"error": "no_table_or_word_index"}
            continue
        if rec["poly_byte"] + 24 > len(poly):
            stats[obj_id] = {"error": "poly_oob", "word_index": f"{rec['word_index']:#x}"}
            continue
        best = None
        attempts = []
        for sw in modes:
            try:
                tris, nf, trunc = _decode(sw)
            except Exception as exc:
                attempts.append({"skip_words": sw, "error": str(exc)})
                continue
            clean = sanitize_tris(tris, max_abs=max_abs)
            mx = mesh_max_abs(clean)
            attempts.append(
                {
                    "skip_words": sw,
                    "tris_raw": len(tris),
                    "tris_clean": len(clean),
                    "nonfinite": nf,
                    "truncated": bool(trunc),
                    "max_abs_clean": mx,
                }
            )
            if not clean:
                continue
            # Prefer more clean tris with in-range extent; primary mode wins ties.
            score = (len(clean), 1 if sw == skip_words else 0, -abs(mx))
            if best is None or score > best[0]:
                best = (score, clean, sw, nf, trunc, mx)
        if best is None:
            meshes[obj_id] = []
            stats[obj_id] = {"error": "no_sane_mesh", "attempts": attempts, "word_index": f"{rec['word_index']:#x}"}
            continue
        _score, clean, sw, nf, trunc, mx = best
        meshes[obj_id] = clean
        stats[obj_id] = {
            "tris": len(clean),
            "nonfinite": nf,
            "truncated": bool(trunc),
            "word_index": f"{rec['word_index']:#x}",
            "w0": f"{rec['w0']:#010x}",
            "w2": f"{rec['w2']:#010x}",
            "w3": f"{rec['w3']:#010x}",
            "decode_skip_words": sw,
            "max_abs": mx,
            "attempts": attempts,
        }
    return meshes, stats


def build_colored_window(
    window: dict[str, Any],
    meshes: dict[int, list],
    palette: dict[int, tuple[int, int, int]],
    *,
    layout: str,
    fifo_fill: bool,
    view: str,
    max_abs: float = SCENE_MAX_ABS,
) -> tuple[list, list[dict[str, Any]]]:
    """Place window meshes into shared world space and project them.

    layout:
      grid      — host-centered AABB then grid offset (analysis assembly)
      raw-grid  — grid offset in ROM local coords (no re-center)
      origin    — all meshes at world origin (no invented translation)
    """
    ids = [i for i in window["ids"] if meshes.get(i)]
    if not ids:
        return [], []
    prepared: dict[int, list] = {}
    for obj_id in ids:
        clean = sanitize_tris(meshes[obj_id], max_abs=max_abs)
        if not clean:
            continue
        if layout in ("grid", "raw-grid") and layout != "raw-grid":
            clean = center_tris(clean)
        prepared[obj_id] = clean
    ids = [i for i in ids if prepared.get(i)]
    if not ids:
        return [], []
    exts = [extent_of_tris(prepared[i]) for i in ids]
    # Robust spacing: median extent (avoid one large object blowing the grid).
    spacing = (sorted(exts)[len(exts) // 2] * 1.6) if exts else 1.0
    spacing = max(spacing, 0.2)
    if layout == "origin":
        offs = [(0.0, 0.0, 0.0)] * len(ids)
        layout_label = "origin (no invented translation)"
    elif layout == "raw-grid":
        offs = grid_offsets(len(ids), spacing)
        layout_label = f"raw_grid spacing={spacing:.4g} (host assembly)"
    else:
        offs = grid_offsets(len(ids), spacing)
        layout_label = f"host_centered_grid spacing={spacing:.4g} (analysis assembly)"

    tag_counts = window.get("tag_counts") or Counter()
    colored: list = []
    legend: list[dict[str, Any]] = []
    for i, obj_id in enumerate(ids):
        rgb, color_src = choose_fill(
            obj_id, palette, tag_counts, fifo_fill=fifo_fill
        )
        dx, dy, dz = offs[i]
        placed = offset_tris(prepared[obj_id], dx, dy, dz)
        proj, _b, _lab = apply_view_to_tris(placed, view)
        label = f"0x{obj_id:03x}"
        for tri in proj:
            colored.append((tri[0], tri[1], tri[2], rgb, label))
        legend.append(
            {
                "id": label,
                "id_int": obj_id,
                "color": list(rgb),
                "color_source": color_src,
                "tris": len(prepared[obj_id]),
                "offset": [dx, dy, dz],
                "layout": layout_label,
            }
        )
    return colored, legend


def render_single_reference(
    ids: list[int],
    meshes: dict[int, list],
    out_dir: Path,
    *,
    size: int,
    view: str,
    palette: dict[int, tuple[int, int, int]],
    max_abs: float = SCENE_MAX_ABS,
) -> list[dict[str, Any]]:
    """Single-object dumps for before/after legibility comparison."""
    files = []
    for obj_id in ids:
        tris = sanitize_tris(meshes.get(obj_id) or [], max_abs=max_abs)
        if not tris:
            continue
        proj, _b, _lab = apply_view_to_tris(tris, view)
        rgb, _cov, _meta = rasterize_scene(
            [(t[0], t[1], t[2], palette.get(obj_id, (92, 148, 210)), f"0x{obj_id:03x}") for t in proj],
            size,
            scale_bounds=None,
        )
        path = out_dir / f"single_id_{obj_id:03x}_{view}_{size}.png"
        written = write_image(path, size, size, rgb)
        files.append({"id": f"0x{obj_id:03x}", "file": written, "tris": len(tris)})
    return files


def main() -> int:
    ap = argparse.ArgumentParser(description="Multi-object attract phase5 scene reconstruction (host analysis)")
    ap.add_argument("--timeline", type=Path, default=Path("out/attr-render/scene/phase5_timeline.jsonl"))
    ap.add_argument("--trace", type=Path, default=Path("out/attr-long/fifo-phase5.jsonl"))
    ap.add_argument("--out", type=Path, default=Path("out/attr-render/scene"))
    ap.add_argument("--main-data", type=Path, default=Path("out/main_data.bin"))
    ap.add_argument("--rom-dir", type=Path, default=DEFAULT_ROM_DIR)
    ap.add_argument("--decode", default="skip3", choices=("skip3", "noskip", "both"))
    ap.add_argument("--view", default="iso-xz")
    ap.add_argument("--window", type=int, default=20, help="object-events per window (event mode)")
    ap.add_argument("--window-mode", default="event", choices=("event", "step"))
    ap.add_argument("--step-bucket", type=int, default=5000, help="step size for step mode")
    ap.add_argument("--size", type=int, default=384, help="window cell size")
    ap.add_argument("--composite-size", type=int, default=1024)
    ap.add_argument("--strip-cell", type=int, default=128)
    ap.add_argument("--layout", default="grid", choices=("grid", "raw-grid", "origin"))
    ap.add_argument("--fifo-fill", action="store_true", help="use FIFO protocol tags as analysis fills")
    ap.add_argument("--bounds", default="global", choices=("global", "first", "auto"),
                    help="shared scale: global projected bounds, first-window, or per-frame auto")
    ap.add_argument("--max-tris", type=int, default=4096, help="per-object decode cap")
    ap.add_argument("--scene-max-abs", type=float, default=SCENE_MAX_ABS,
                    help="reject decode coords beyond this magnitude (scene sanity)")
    ap.add_argument("--no-mode-fallback", action="store_true",
                    help="disable per-object skip3/noskip fallback when primary decode is extreme")
    ap.add_argument("--max-objects", type=int, default=32, help="max objects per scene/composite")
    ap.add_argument("--max-windows", type=int, default=12, help="max temporal windows to render")
    ap.add_argument("--max-total-tris", type=int, default=40000, help="safety cap for composite")
    ap.add_argument("--timeline-only", action="store_true", help="build timeline then exit")
    ap.add_argument("--composite-only", action="store_true")
    ap.add_argument("--build-timeline", action="store_true", help="force rebuild timeline from --trace")
    ap.add_argument("--no-singles", action="store_true")
    args = ap.parse_args()

    out_dir = args.out
    out_dir.mkdir(parents=True, exist_ok=True)

    # Build or load timeline.
    if args.build_timeline or not args.timeline.is_file():
        from build_phase5_timeline import build_timeline

        table0 = load_object_table(main_data_path=args.main_data, rom_dir=args.rom_dir)
        if not args.trace.is_file():
            print(f"ERROR: trace missing for timeline build: {args.trace}", file=sys.stderr)
            return 2
        print(f"building timeline from {args.trace} ...")
        build_timeline(args.trace, args.timeline, table=table0)
        if args.timeline_only:
            print(f"timeline written: {args.timeline}")
            return 0

    events, tl_summary = load_timeline(args.timeline)
    if not events:
        print(f"ERROR: no object_events in {args.timeline}", file=sys.stderr)
        return 2
    print(
        f"timeline events={len(events)} unique_ids={tl_summary.get('unique_id_count')} "
        f"steps={tl_summary.get('step_min')}..{tl_summary.get('step_max')}"
    )

    # Object table + poly ROM.
    import render_poly_objects as rpo_mod

    rpo_mod.ROM_DIR = args.rom_dir
    table = load_object_table(main_data_path=args.main_data, rom_dir=args.rom_dir)
    by_id = table["by_id"]
    poly = load_poly(rom_dir=args.rom_dir)
    print(f"table by_id={len(by_id)} poly={len(poly):#x}")

    decode_fn = import_decode_object()
    skip_words = 3 if args.decode in ("skip3", "both") else 0
    if args.decode == "noskip":
        skip_words = 0
    print(f"decode={args.decode} skip_words={skip_words} view={args.view} layout={args.layout}")

    # Unique ids ordered by first appearance.
    unique_ids: list[int] = []
    seen: set[int] = set()
    for ev in events:
        iid = ev.get("id_int")
        if iid is None and ev.get("id"):
            iid = int(str(ev["id"]), 16)
        if iid is not None and int(iid) not in seen:
            seen.add(int(iid))
            unique_ids.append(int(iid))
    unique_ids = unique_ids
    palette = index_palette(range(0x2000))

    # Decode every unique timeline id so later windows keep their meshes.
    # Per-scene object count is still capped by --max-objects at render time.
    print(f"decoding {len(unique_ids)} unique objects (skip_words={skip_words}) ...")
    meshes, decode_stats = decode_objects(
        unique_ids,
        by_id,
        poly,
        decode_fn,
        skip_words=skip_words,
        max_tris=args.max_tris,
        auto_mode_fallback=not args.no_mode_fallback,
        max_abs=args.scene_max_abs,
    )
    mesh_ok = [i for i in unique_ids if meshes.get(i)]
    print(f"decoded with tris: {len(mesh_ok)} / {len(unique_ids)}")

    # Windows.
    windows = group_windows(
        events,
        mode=args.window_mode,
        window=args.window,
        step_bucket=args.step_bucket,
    )
    print(f"windows={len(windows)} (render up to {args.max_windows})")

    # Shared scale: prefer composite placement bounds so every frame shares
    # one camera scale that fits the multi-object grid (not each mesh alone).
    scale_bounds = None
    bounds_policy = args.bounds

    # Pre-select composite ids (needed for global scale).
    all_mesh_ids_pre = [i for i in unique_ids if meshes.get(i)]
    known_families_pre: list[int] = []
    for fam in (
        range(0x88, 0x89),
        range(0x140, 0x158),
        range(0x4D1, 0x4EC),
        {0x503, 0x750, 0xF5D, 0xEE1},
        range(0xD06, 0xD12),
        range(0x298, 0x2A5),
        range(0x8F7, 0x907),
        range(0xF68, 0xF77),
    ):
        for fid in fam:
            if fid in meshes and meshes.get(fid) and fid not in known_families_pre:
                known_families_pre.append(fid)
    if len(known_families_pre) >= args.max_objects:
        composite_ids = known_families_pre[: args.max_objects]
    else:
        fill = [i for i in all_mesh_ids_pre if i not in set(known_families_pre)]
        fill.sort(key=lambda i: (-len(meshes.get(i) or []), i))
        composite_ids = (known_families_pre + fill)[: args.max_objects]

    def placed_bounds_for(ids_use: list[int], view_name: str):
        prep = {}
        for i in ids_use:
            clean = sanitize_tris(meshes[i], max_abs=args.scene_max_abs)
            if not clean:
                continue
            prep[i] = center_tris(clean) if args.layout == "grid" else clean
        if not prep:
            return None
        fake = {
            "ids": list(prep.keys()),
            "tag_counts": Counter(),
            "event_count": 0,
        }
        colored, _leg = build_colored_window(
            fake,
            prep,
            palette,
            layout=args.layout,
            fifo_fill=False,
            view=view_name,
            max_abs=args.scene_max_abs,
        )
        if not colored:
            return None
        sxs = [p[0] for t in colored for p in t[:3]]
        sys_ = [p[1] for t in colored for p in t[:3]]
        if not sxs:
            return None
        return (min(sxs), max(sxs), min(sys_), max(sys_))

    if bounds_policy in ("global", "first"):
        if bounds_policy == "global":
            src_ids = composite_ids or mesh_ok
        else:
            src_ids = []
            for w in windows:
                cand = [i for i in w["ids"] if meshes.get(i)]
                if cand:
                    src_ids = cand[: args.max_objects]
                    break
        scale_bounds = placed_bounds_for(src_ids, args.view)
        if scale_bounds is None:
            scale_bounds = project_bounds_all(
                {
                    i: (center_tris(meshes[i]) if args.layout == "grid" else meshes[i])
                    for i in (src_ids or mesh_ok)
                    if meshes.get(i)
                },
                args.view,
                max_abs=args.scene_max_abs,
            )
        print(f"shared scale bounds ({bounds_policy}): {scale_bounds}")

    report_windows = []
    strip_cells: list[tuple[str, bytes, int]] = []
    best_scene = None
    windows_to_render = windows if args.composite_only else windows[: args.max_windows]

    if not args.composite_only:
        for w in windows_to_render:
            # Cap objects per window for legibility / runtime.
            w_ids = w["ids"][: args.max_objects]
            w = {**w, "ids": w_ids}
            wmesh = {i: meshes[i] for i in w["ids"] if meshes.get(i)}
            if not wmesh:
                continue
            colored, legend = build_colored_window(
                w,
                meshes,
                palette,
                layout=args.layout,
                fifo_fill=args.fifo_fill,
                view=args.view,
            )
            if not colored:
                continue
            sb = scale_bounds
            if bounds_policy == "auto":
                sb = None
            elif bounds_policy == "first" and sb is None:
                sxs = [p[0] for t in colored for p in t[:3]]
                sys_ = [p[1] for t in colored for p in t[:3]]
                sb = (min(sxs), max(sxs), min(sys_), max(sys_))
            rgb, cov, meta = rasterize_scene(colored, args.size, scale_bounds=sb)
            fname = out_dir / f"phase5_w{w['index']:03d}_iso_{args.size}.png"
            written = write_image(fname, args.size, args.size, rgb)
            rec = {
                "window": w["index"],
                "file": written,
                "ids": [f"0x{i:03x}" for i in w["ids"]],
                "object_count": len(w["ids"]),
                "mesh_object_count": len(wmesh),
                "event_count": w["event_count"],
                "step_min": w["step_min"],
                "step_max": w["step_max"],
                "kind_counts": w["kind_counts"],
                "coverage_px": cov,
                "tri_count": meta.get("tri_count"),
                "legend": legend,
                "scale_bounds": meta.get("scale_bounds"),
                "layout_mode": args.layout,
                "color_policy": "fifo_analysis_fill" if args.fifo_fill else "per_id_palette",
            }
            report_windows.append(rec)
            label = f"w{w['index']:03d} n={len(w['ids'])} e={w['event_count']}"
            strip_cells.append((label, rgb, args.size))
            if best_scene is None or cov > (best_scene.get("coverage_px") or 0):
                best_scene = rec
            print(
                f"  window {w['index']:03d}: ids={len(w['ids'])} events={w['event_count']} "
                f"cov={cov} -> {written}"
            )

    # Composite mega-scene: reuse composite_ids selected for shared scale.
    print("building composite mega-scene ...")
    total_tris = 0
    capped_ids = []
    for i in composite_ids:
        n = len(meshes.get(i) or [])
        if total_tris + n > args.max_total_tris and capped_ids:
            break
        capped_ids.append(i)
        total_tris += n
    composite_ids = capped_ids

    total_tris = 0
    capped_ids = []
    for i in composite_ids:
        n = len(meshes[i])
        if total_tris + n > args.max_total_tris and capped_ids:
            break
        capped_ids.append(i)
        total_tris += n
    composite_ids = capped_ids

    fake_win = {
        "ids": composite_ids,
        "tag_counts": Counter(),
        "event_count": sum(
            1 for ev in events if (ev.get("id_int") in set(composite_ids))
        ),
    }
    composite_files = []
    for view_name in ("iso-xz", "top"):
        colored, legend = build_colored_window(
            fake_win,
            meshes,
            palette,
            layout=args.layout,
            fifo_fill=args.fifo_fill,
            view=view_name,
            max_abs=args.scene_max_abs,
        )
        if not colored:
            continue
        size = args.composite_size
        if args.bounds != "auto" and view_name == args.view and scale_bounds is not None:
            sb = scale_bounds
        else:
            comp_src = {}
            for i in composite_ids:
                clean = sanitize_tris(meshes[i], max_abs=args.scene_max_abs)
                if not clean:
                    continue
                comp_src[i] = center_tris(clean) if args.layout == "grid" else clean
            sb = project_bounds_all(comp_src, view_name, max_abs=args.scene_max_abs)
        rgb, cov, meta = rasterize_scene(colored, size, scale_bounds=sb)
        path = out_dir / f"phase5_composite_{view_name}_{size}.png"
        written = write_image(path, size, size, rgb)
        composite_files.append(
            {
                "view": view_name,
                "file": written,
                "ids": [f"0x{i:03x}" for i in composite_ids],
                "object_count": len(composite_ids),
                "tri_count": meta.get("tri_count"),
                "coverage_px": cov,
                "scale_bounds": meta.get("scale_bounds"),
                "legend_count": len(legend),
            }
        )
        print(f"  composite {view_name}: objects={len(composite_ids)} tris={meta.get('tri_count')} cov={cov} -> {written}")

    # Single-object comparison dumps (before legibility baseline).
    single_files = []
    if not args.no_singles and not args.composite_only:
        compare_ids = composite_ids[:4]
        single_files = render_single_reference(
            compare_ids,
            meshes,
            out_dir,
            size=args.size,
            view=args.view,
            palette=palette,
            max_abs=args.scene_max_abs,
        )
        print(f"single-object comparison dumps: {len(single_files)}")

    # Contact strip.
    strip_path = ""
    if strip_cells:
        strip_path = draw_strip(
            out_dir / "phase5_strip",
            strip_cells,
            cell=args.strip_cell,
        )
        print(f"strip: {strip_path}")

    # Legibility note (before/after).
    legibility = {
        "before": "single-object dumps: one mesh per PNG, no temporal co-occurrence, no palette identity",
        "after": (
            "multi-object windows show measured co-submitted ids together under a shared "
            "analysis camera/scale; per-id palette (and optional FIFO analysis fills) make "
            "object identity and submit order more legible than isolated meshes"
        ),
        "single_files": single_files,
        "best_window_file": (best_scene or {}).get("file"),
        "composite_files": composite_files,
        "caveat": (
            "Composite is host assembly from measured submits + poly-ROM decode. "
            "Not a game framebuffer. Not CRT output. Transforms absent → no recovered camera."
        ),
    }

    report = {
        "tool": "render_scene_attract",
        "phase": "P3 phase5 multi-object scene",
        "disclaimer": (
            "Host analysis reconstruction from measured object-table submits. "
            "NOT recovered C, NOT CRT/framebuffer, NOT a named logo/title witness."
        ),
        "fail_closed": {
            "logo_named": False,
            "crt_claim": False,
            "transforms_invented": False,
            "composite_is_host_assembly": True,
            "fifo_tags_analysis_fills_not_rgb": True,
        },
        "color_policy": {
            "default": "per_id_palette (golden-angle HSV; analysis legibility)",
            "fifo_fill": args.fifo_fill,
            "fifo_family_examples": ["0x14802929", "0x1c803939", "0x03000606", "0x09801313", "0x33806767"],
            "note": "FIFO words are protocol tags, NOT proven RGB; used only as labeled analysis fills",
        },
        "view": args.view,
        "decode": args.decode,
        "layout": args.layout,
        "bounds_policy": args.bounds,
        "scene_max_abs": args.scene_max_abs,
        "mode_fallback": not args.no_mode_fallback,
        "size": args.size,
        "composite_size": args.composite_size,
        "timeline": str(args.timeline),
        "timeline_summary": {
            k: tl_summary.get(k)
            for k in (
                "object_events",
                "table_reads",
                "submits",
                "fifo_tags",
                "unique_id_count",
                "unique_ids",
                "id_counts",
                "port_write_counts",
                "tag_values_top",
                "step_min",
                "step_max",
            )
        },
        "unique_ids_rendered": [f"0x{i:03x}" for i in unique_ids],
        "decoded_ok": [f"0x{i:03x}" for i in mesh_ok],
        "decode_stats": {f"0x{i:03x}": decode_stats.get(i) for i in unique_ids},
        "windows": report_windows,
        "composite": composite_files,
        "strip": strip_path,
        "legibility": legibility,
        "outputs_dir": str(out_dir).replace("\\", "/"),
        "git_note": "out/ is gitignored; commit only tools + decomp note",
    }
    report_path = out_dir / "phase5_scene_report.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"report: {report_path}")
    print(
        f"done: windows={len(report_windows)} composite={len(composite_files)} "
        f"strip={bool(strip_path)} unique_ids={tl_summary.get('unique_id_count')}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
