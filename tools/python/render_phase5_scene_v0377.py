#!/usr/bin/env python3
"""P3/P4 v0377 deliverable: multi-object attract phase5 scene PNG strips.

Host analysis only. Fail-closed logo: numeric object ids, unnamed meshes,
no CRT/framebuffer claim. Transforms confidence=absent → layout labeled
'no_scene_matrix' (grid / sequential host assembly).

Produces under --out (default out/attr-render/v0377/scene/):
  phase5_timeline.json          ordered submits + color-tag table
  scene_grid_ids.png            all measured phase5 ids, protocol colors
  scene_grid_ids_wire.png       same layout, muted wire/shaded (no color)
  scene_strip_t00..tN.png       up to 8 temporal snapshots (protocol colors)
  scene_strip_wire_t00.png      representative wire/shaded temporal frame
  scene_contact.png             contact sheet (grid + strips + compare)
  scene_compare_color_vs_wire.png  side-by-side color vs wire
  phase5_scene_report.json      machine-readable report

Color policy: every FIFO family use is confidence=protocol_tag.
Uncorrelated ids fall back to muted gray/blue (not invented palette).
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent))

from scene_decode_helper import (  # noqa: E402
    DEFAULT_ROM_DIR,
    import_decode_object,
    index_palette,
    is_protocol_color_like,
    load_object_table,
    load_poly,
    protocol_tag_decode,
    protocol_tag_rgb,
    try_import_pil,
)
from render_scene_attract import (  # noqa: E402
    SCENE_MAX_ABS,
    apply_view_to_tris,
    build_colored_window,
    center_tris,
    decode_objects,
    draw_strip,
    group_windows,
    load_timeline,
    rasterize_scene,
    sanitize_tris,
    write_image,
)

MUTED_UNCORRELATED = (88, 100, 122)
MUTED_WIRE = (148, 156, 168)
EDGE_DARK = (28, 32, 40)


def build_ordered_timeline(
    events: list[dict[str, Any]],
    summary: dict[str, Any],
    *,
    out_path: Path,
) -> dict[str, Any]:
    """Emit phase5_timeline.json: ordered submits + id↔tag color mapping."""
    id_tag_counts: dict[int, Counter] = defaultdict(Counter)
    id_order: list[int] = []
    seen: set[int] = set()
    id_first_step: dict[int, int] = {}
    id_kind_counts: dict[int, Counter] = defaultdict(Counter)
    ordered_submits: list[dict[str, Any]] = []
    global_tag_counts: Counter = Counter()

    for ev in events:
        iid = ev.get("id_int")
        if iid is None and ev.get("id"):
            try:
                iid = int(str(ev["id"]), 16)
            except ValueError:
                continue
        if iid is None:
            continue
        iid = int(iid)
        step = int(ev.get("step") or 0)
        kind = ev.get("kind") or "?"
        id_kind_counts[iid][kind] += 1
        if iid not in seen:
            seen.add(iid)
            id_order.append(iid)
            id_first_step[iid] = step
        near_protocol = []
        for nt in ev.get("near_tags") or []:
            try:
                tv = int(str(nt.get("w0") or "0"), 16)
            except ValueError:
                continue
            if is_protocol_color_like(tv):
                id_tag_counts[iid][tv] += 1
                global_tag_counts[tv] += 1
                near_protocol.append(f"0x{tv:08x}")

        # Color assignment for this event (protocol_tag or muted default).
        if id_tag_counts[iid]:
            best = id_tag_counts[iid].most_common(1)[0][0]
            meta = protocol_tag_decode(best)
            rgb = protocol_tag_rgb(best)
            color_rec = {
                "word": f"0x{best:08x}",
                "rgb_analysis": list(rgb) if rgb else None,
                "confidence": "protocol_tag",
                "game_accurate_palette": False,
                "tag_high": meta["tag_high"] if meta else None,
                "hypothesis": meta["hypothesis"] if meta else None,
            }
        else:
            color_rec = {
                "word": None,
                "rgb_analysis": list(MUTED_UNCORRELATED),
                "confidence": "muted_default_uncorrelated",
                "game_accurate_palette": False,
                "tag_high": None,
                "hypothesis": "no nearby protocol tag; muted gray/blue host fill",
            }

        rec = {
            "order": len(ordered_submits),
            "step": step,
            "kind": kind,
            "id": f"0x{iid:03x}",
            "id_int": iid,
            "w0": ev.get("w0"),
            "word_index": ev.get("word_index"),
            "addr": ev.get("addr"),
            "port": ev.get("port"),
            "near_protocol_tags": near_protocol,
            "color": color_rec,
        }
        ordered_submits.append(rec)

    # Per-id color mapping table (first-seen order).
    id_color_table = []
    for iid in id_order:
        tags = id_tag_counts.get(iid) or Counter()
        if tags:
            best, n = tags.most_common(1)[0]
            meta = protocol_tag_decode(best)
            rgb = protocol_tag_rgb(best)
            id_color_table.append(
                {
                    "id": f"0x{iid:03x}",
                    "id_int": iid,
                    "first_step": id_first_step.get(iid),
                    "dominant_tag": f"0x{best:08x}",
                    "dominant_tag_n": n,
                    "all_tags": [
                        {"word": f"0x{k:08x}", "n": v} for k, v in tags.most_common()
                    ],
                    "rgb_analysis": list(rgb) if rgb else list(MUTED_UNCORRELATED),
                    "confidence": "protocol_tag",
                    "game_accurate_palette": False,
                    "hypothesis": meta["hypothesis"] if meta else None,
                    "tag_high": meta["tag_high"] if meta else None,
                    "kind_counts": dict(id_kind_counts.get(iid) or {}),
                }
            )
        else:
            id_color_table.append(
                {
                    "id": f"0x{iid:03x}",
                    "id_int": iid,
                    "first_step": id_first_step.get(iid),
                    "dominant_tag": None,
                    "dominant_tag_n": 0,
                    "all_tags": [],
                    "rgb_analysis": list(MUTED_UNCORRELATED),
                    "confidence": "muted_default_uncorrelated",
                    "game_accurate_palette": False,
                    "hypothesis": "no protocol tag correlated in ±window",
                    "tag_high": None,
                    "kind_counts": dict(id_kind_counts.get(iid) or {}),
                }
            )

    # Global color-tag table from measured words.
    color_tag_table = []
    for tv, n in global_tag_counts.most_common():
        meta = protocol_tag_decode(tv)
        if meta is None:
            continue
        meta = dict(meta)
        meta["measured_count"] = n
        meta["rgb_analysis"] = list(protocol_tag_rgb(tv) or (0, 0, 0))
        color_tag_table.append(meta)

    # Explicit task-listed tags even if rare (fail-closed documentation).
    task_listed = [
        0x00800101,
        0x01800303,
        0x03000606,
        0x1A003434,
        0x14802929,
        0x1C803939,
        0x33806767,
        0x34806969,
        0x35806B6B,
        0x36006C6C,
    ]
    listed_present = {int(e["word_int"]) for e in color_tag_table}
    for tv in task_listed:
        if tv in listed_present:
            continue
        meta = protocol_tag_decode(tv)
        if meta is None:
            continue
        meta = dict(meta)
        meta["measured_count"] = global_tag_counts.get(tv, 0)
        meta["rgb_analysis"] = list(protocol_tag_rgb(tv) or (0, 0, 0))
        meta["note"] = "listed in measured FIFO protocol family (task/handoff)"
        color_tag_table.append(meta)

    doc = {
        "schema": "vf2_phase5_timeline_v0377",
        "tool": "render_phase5_scene_v0377 / build_phase5_timeline",
        "source_trace": summary.get("source"),
        "source_timeline_jsonl": summary.get("output"),
        "disclaimer": (
            "Host analysis timeline from measured object-table reads + geo/FIFO "
            "word0 reverse-map. NOT recovered C. NOT CRT output. Unnamed ids."
        ),
        "fail_closed": {
            "logo_named": False,
            "meshes_named": False,
            "crt_claim": False,
            "scene_matrix": "absent",
            "layout_label": "no_scene_matrix",
            "transforms_invented": False,
            "fifo_tags_confidence": "protocol_tag",
            "game_accurate_palette": False,
        },
        "table": {
            "base": "0x020e0004",
            "record_size": 16,
            "w0_field": "w0 written to geo 0x800010",
            "poly_word_index": "w2 & 0x7fffff",
        },
        "color_tag_hypothesis": (
            "Measured words follow 0xAABBCCDD with BB in {0x00,0x80} and often "
            "CC==DD. Hypothesis: high AA:BB is a protocol tag; CC==DD is an "
            "intensity (r=g=CC). Every use labeled confidence=protocol_tag. "
            "NOT a recovered palette."
        ),
        "color_tag_table": color_tag_table,
        "ordered_submits": ordered_submits,
        "id_color_table": id_color_table,
        "first_submit_id_order": [f"0x{i:03x}" for i in id_order],
        "summary": {
            "object_events": len(ordered_submits),
            "unique_ids": len(id_order),
            "table_reads": summary.get("table_reads"),
            "submits": summary.get("submits"),
            "fifo_tags": summary.get("fifo_tags"),
            "step_min": summary.get("step_min"),
            "step_max": summary.get("step_max"),
            "id_counts": summary.get("id_counts"),
            "tag_values_top": summary.get("tag_values_top"),
            "protocol_tag_unique": len(global_tag_counts),
        },
    }
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(doc, indent=2), encoding="utf-8")
    return doc


def rasterize_wire(
    colored_tris,
    size: int,
    *,
    scale_bounds,
):
    """Wire/shaded mode: all faces muted gray, darker edge highlight."""
    wire_tris = []
    for tri in colored_tris:
        p0, p1, p2 = tri[0], tri[1], tri[2]
        lab = tri[4] if len(tri) > 4 else ""
        wire_tris.append((p0, p1, p2, MUTED_WIRE, lab))
    rgb, cov, meta = rasterize_scene(wire_tris, size, scale_bounds=scale_bounds)
    # Edge pass: redraw triangle borders darker for wire readability.
    Image, _D, _F = try_import_pil()
    if Image is None or not rgb or not wire_tris:
        return rgb, cov, meta
    img = Image.frombytes("RGB", (size, size), rgb)
    draw = _D.Draw(img)
    minx, maxx, miny, maxy = meta.get("scale_bounds") or (
        0.0, 1.0, 0.0, 1.0
    )
    spanx = max(maxx - minx, 1e-9)
    spany = max(maxy - miny, 1e-9)
    margin = 0.05
    usable = 1.0 - 2.0 * margin
    scale = min(usable * (size - 1) / spanx, usable * (size - 1) / spany)
    ox = (size - 1) * 0.5 - 0.5 * scale * (minx + maxx)
    oy = (size - 1) * 0.5 + 0.5 * scale * (miny + maxy)

    def to_screen(p):
        return (p[0] * scale + ox, oy - p[1] * scale)

    for tri in wire_tris:
        pts = [to_screen(tri[0]), to_screen(tri[1]), to_screen(tri[2])]
        draw.line([pts[0], pts[1]], fill=EDGE_DARK, width=1)
        draw.line([pts[1], pts[2]], fill=EDGE_DARK, width=1)
        draw.line([pts[2], pts[0]], fill=EDGE_DARK, width=1)
    return img.tobytes(), cov, meta


def draw_side_by_side(
    path: Path,
    left_rgb: bytes,
    right_rgb: bytes,
    size: int,
    *,
    left_label: str,
    right_label: str,
) -> str:
    Image, ImageDraw, ImageFont = try_import_pil()
    if Image is None:
        return ""
    pad = 8
    label_h = 24
    w = pad * 3 + size * 2
    h = pad * 2 + size + label_h
    sheet = Image.new("RGB", (w, h), (12, 14, 20))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.load_default()
    except Exception:
        font = None
    for i, (rgb, label) in enumerate(
        ((left_rgb, left_label), (right_rgb, right_label))
    ):
        x0 = pad + i * (size + pad)
        if rgb:
            img = Image.frombytes("RGB", (size, size), rgb)
            sheet.paste(img, (x0, pad))
        draw.text((x0 + 2, pad + size + 4), label[:56], fill=(200, 210, 230), font=font)
    out = path.with_suffix(".png")
    sheet.save(out)
    return str(out)


def draw_contact(
    path: Path,
    cells: list[tuple[str, bytes, int]],
    *,
    cols: int = 3,
    cell: int = 256,
) -> str:
    Image, ImageDraw, ImageFont = try_import_pil()
    if Image is None or not cells:
        return ""
    pad = 6
    label_h = 20
    rows = int(math.ceil(len(cells) / cols))
    w = pad + cols * (cell + pad)
    h = pad + rows * (cell + label_h + pad)
    sheet = Image.new("RGB", (w, h), (10, 12, 18))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.load_default()
    except Exception:
        font = None
    for i, (label, rgb, size) in enumerate(cells):
        r, c = divmod(i, cols)
        x0 = pad + c * (cell + pad)
        y0 = pad + r * (cell + label_h + pad)
        if rgb and size > 0:
            img = Image.frombytes("RGB", (size, size), rgb)
            if size != cell:
                img = img.resize((cell, cell))
            sheet.paste(img, (x0, y0))
        draw.text((x0 + 2, y0 + cell + 2), label[:48], fill=(190, 200, 220), font=font)
    out = path.with_suffix(".png")
    sheet.save(out)
    return str(out)


def color_for_id(
    obj_id: int,
    id_tags: dict[int, Counter],
    palette: dict[int, tuple[int, int, int]],
    *,
    prefer_protocol: bool,
) -> tuple[tuple[int, int, int], str, str]:
    """Protocol-tag fill when id-correlated; else muted gray/blue.

    When many ids share one ambient dominant tag, tint with per-id analysis
    palette so multi-object sheets stay separable. Still confidence=protocol_tag
    (tag-derived host fill, not game palette).
    """
    tags = id_tags.get(obj_id) or Counter()
    if tags:
        best, _n = tags.most_common(1)[0]
        rgb = protocol_tag_rgb(best)
        meta = protocol_tag_decode(best)
        if rgb is not None:
            base = palette.get(obj_id, MUTED_UNCORRELATED)
            if prefer_protocol:
                # 70% protocol tag + 30% per-id palette tint (legibility).
                mixed = (
                    int(rgb[0] * 0.70 + base[0] * 0.30),
                    int(rgb[1] * 0.70 + base[1] * 0.30),
                    int(rgb[2] * 0.70 + base[2] * 0.30),
                )
                return (
                    mixed,
                    f"protocol_tag:{meta['word']}+id_tint",
                    "protocol_tag",
                )
            mixed = (
                (rgb[0] + base[0]) // 2,
                (rgb[1] + base[1]) // 2,
                (rgb[2] + base[2]) // 2,
            )
            return mixed, f"protocol_tag_blend:{meta['word']}", "protocol_tag"
    if prefer_protocol:
        return (
            MUTED_UNCORRELATED,
            "muted_gray_blue_uncorrelated",
            "muted_default_uncorrelated",
        )
    return (
        palette.get(obj_id, MUTED_UNCORRELATED),
        "per_id_palette_analysis",
        "analysis_palette",
    )


def place_window_meshes(
    ids: list[int],
    meshes: dict[int, list],
    id_tags: dict[int, Counter],
    palette: dict[int, tuple[int, int, int]],
    *,
    view: str,
    layout: str,
    prefer_protocol: bool,
    wire: bool,
    max_abs: float = SCENE_MAX_ABS,
) -> tuple[list, list[dict[str, Any]]]:
    prepared: dict[int, list] = {}
    use_ids = []
    for obj_id in ids:
        clean = sanitize_tris(meshes.get(obj_id) or [], max_abs=max_abs)
        if not clean:
            continue
        if layout == "grid":
            clean = center_tris(clean)
        prepared[obj_id] = clean
        use_ids.append(obj_id)
    if not use_ids:
        return [], []
    exts = []
    for obj_id in use_ids:
        tris = prepared[obj_id]
        xs = [p[0] for t in tris for p in t]
        ys = [p[1] for t in tris for p in t]
        zs = [p[2] for t in tris for p in t]
        exts.append(max(max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs), 1e-6))
    spacing = max(sorted(exts)[len(exts) // 2] * 1.6, 0.2)
    n = len(use_ids)
    cols = max(1, int(math.ceil(math.sqrt(n))))
    rows = int(math.ceil(n / cols))
    colored = []
    legend = []
    for i, obj_id in enumerate(use_ids):
        r_i, c_i = divmod(i, cols)
        dx = (c_i - (cols - 1) * 0.5) * spacing
        dy = 0.0
        dz = (r_i - (rows - 1) * 0.5) * spacing
        placed = [
            tuple((p[0] + dx, p[1] + dy, p[2] + dz) for p in t)
            for t in prepared[obj_id]
        ]
        proj, _b, _lab = apply_view_to_tris(placed, view)
        if wire:
            rgb, src, conf = MUTED_WIRE, "wire_shaded_no_color", "wire"
        else:
            rgb, src, conf = color_for_id(
                obj_id, id_tags, palette, prefer_protocol=prefer_protocol
            )
        label = f"0x{obj_id:03x}"
        for tri in proj:
            colored.append((tri[0], tri[1], tri[2], rgb, label))
        tags = id_tags.get(obj_id) or Counter()
        legend.append(
            {
                "id": label,
                "id_int": obj_id,
                "color": list(rgb),
                "color_source": src,
                "confidence": conf,
                "tris": len(prepared[obj_id]),
                "offset": [dx, dy, dz],
                "layout": "no_scene_matrix/grid_host_assembly",
                "protocol_tags": [
                    {"word": f"0x{k:08x}", "n": v} for k, v in tags.most_common(4)
                ],
            }
        )
    return colored, legend


def main() -> int:
    ap = argparse.ArgumentParser(
        description="v0377 multi-object attract phase5 scene deliverable (host analysis)"
    )
    ap.add_argument(
        "--timeline",
        type=Path,
        default=Path("out/attr-render/scene/phase5_timeline.jsonl"),
    )
    ap.add_argument("--trace", type=Path, default=Path("out/attr-long/fifo-phase5.jsonl"))
    ap.add_argument(
        "--out",
        type=Path,
        default=Path("out/attr-render/v0377/scene"),
    )
    ap.add_argument("--main-data", type=Path, default=Path("out/main_data.bin"))
    ap.add_argument("--rom-dir", type=Path, default=DEFAULT_ROM_DIR)
    ap.add_argument("--view", default="iso-xz")
    ap.add_argument("--decode", default="skip3", choices=("skip3", "noskip", "both"))
    ap.add_argument("--grid-size", type=int, default=1600)
    ap.add_argument("--strip-size", type=int, default=384)
    ap.add_argument("--contact-cell", type=int, default=220)
    ap.add_argument("--max-strips", type=int, default=8)
    ap.add_argument("--events-per-strip", type=int, default=0,
                    help="0 = auto: ceil(object_events / max_strips)")
    ap.add_argument("--max-grid-objects", type=int, default=112)
    ap.add_argument("--max-tris", type=int, default=4096)
    ap.add_argument("--scene-max-abs", type=float, default=SCENE_MAX_ABS)
    ap.add_argument("--rebuild-timeline", action="store_true")
    args = ap.parse_args()

    out_dir = args.out
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.rebuild_timeline or not args.timeline.is_file():
        from build_phase5_timeline import build_timeline

        table0 = load_object_table(main_data_path=args.main_data, rom_dir=args.rom_dir)
        if not args.trace.is_file():
            print(f"ERROR: trace missing: {args.trace}", file=sys.stderr)
            return 2
        print(f"building timeline from {args.trace} ...")
        build_timeline(args.trace, args.timeline, table=table0)

    events, tl_summary = load_timeline(args.timeline)
    if not events:
        print(f"ERROR: no object_events in {args.timeline}", file=sys.stderr)
        return 2
    print(
        f"timeline events={len(events)} unique={tl_summary.get('unique_id_count')} "
        f"steps={tl_summary.get('step_min')}..{tl_summary.get('step_max')}"
    )

    timeline_json = out_dir / "phase5_timeline.json"
    tl_doc = build_ordered_timeline(events, tl_summary, out_path=timeline_json)
    print(f"wrote {timeline_json} submits={len(tl_doc['ordered_submits'])}")

    id_order = [
        int(s, 16) for s in tl_doc["first_submit_id_order"]
    ]
    id_tags: dict[int, Counter] = {}
    for row in tl_doc["id_color_table"]:
        tags = Counter()
        for t in row.get("all_tags") or []:
            tags[int(t["word"], 16)] = int(t.get("n") or 0)
        if tags:
            id_tags[int(row["id_int"])] = tags

    table = load_object_table(main_data_path=args.main_data, rom_dir=args.rom_dir)
    by_id = table["by_id"]
    poly = load_poly(rom_dir=args.rom_dir)
    decode_fn = import_decode_object()
    skip_words = 0 if args.decode == "noskip" else 3
    print(f"decoding {len(id_order)} objects skip_words={skip_words} ...")
    meshes, decode_stats = decode_objects(
        id_order,
        by_id,
        poly,
        decode_fn,
        skip_words=skip_words,
        max_tris=args.max_tris,
        auto_mode_fallback=True,
        max_abs=args.scene_max_abs,
    )
    mesh_ok = [i for i in id_order if meshes.get(i)]
    print(f"decoded ok: {len(mesh_ok)} / {len(id_order)}")

    palette = index_palette(range(0x2000))

    # --- Grid: all measured ids, protocol colors ---
    grid_ids = [i for i in mesh_ok[: args.max_grid_objects]]
    colored_grid, legend_grid = place_window_meshes(
        grid_ids,
        meshes,
        id_tags,
        palette,
        view=args.view,
        layout="grid",
        prefer_protocol=True,
        wire=False,
        max_abs=args.scene_max_abs,
    )
    scale_bounds = None
    if colored_grid:
        sxs = [p[0] for t in colored_grid for p in t[:3]]
        sys_ = [p[1] for t in colored_grid for p in t[:3]]
        if sxs:
            scale_bounds = (min(sxs), max(sxs), min(sys_), max(sys_))

    grid_path = ""
    grid_wire_path = ""
    grid_rgb = b""
    grid_wire_rgb = b""
    if colored_grid:
        grid_rgb, cov_g, meta_g = rasterize_scene(
            colored_grid, args.grid_size, scale_bounds=scale_bounds
        )
        grid_path = write_image(
            out_dir / "scene_grid_ids.png", args.grid_size, args.grid_size, grid_rgb
        )
        print(f"grid color: objects={len(grid_ids)} cov={cov_g} -> {grid_path}")

        colored_wire, legend_wire = place_window_meshes(
            grid_ids,
            meshes,
            id_tags,
            palette,
            view=args.view,
            layout="grid",
            prefer_protocol=True,
            wire=True,
            max_abs=args.scene_max_abs,
        )
        grid_wire_rgb, cov_w, meta_w = rasterize_wire(
            colored_wire, args.grid_size, scale_bounds=scale_bounds
        )
        grid_wire_path = write_image(
            out_dir / "scene_grid_ids_wire.png",
            args.grid_size,
            args.grid_size,
            grid_wire_rgb,
        )
        print(f"grid wire: cov={cov_w} -> {grid_wire_path}")

    compare_path = ""
    if grid_rgb and grid_wire_rgb:
        compare_path = draw_side_by_side(
            out_dir / "scene_compare_color_vs_wire",
            grid_rgb,
            grid_wire_rgb,
            args.grid_size,
            left_label="protocol_tag colors (not game palette)",
            right_label="wire/shaded muted (no color)",
        )
        print(f"compare: {compare_path}")

    # --- Temporal strips (up to 8) ---
    n_events = len(events)
    per = args.events_per_strip
    if per <= 0:
        per = max(1, int(math.ceil(n_events / max(1, args.max_strips))))
    windows = group_windows(events, mode="event", window=per, step_bucket=0)
    windows = windows[: args.max_strips]
    print(f"temporal windows: {len(windows)} events_per={per}")

    strip_cells: list[tuple[str, bytes, int]] = []
    strip_files = []
    strip_wire_rgb = b""
    strip_wire_path = ""
    for w in windows:
        w_ids = [i for i in w["ids"] if meshes.get(i)][: args.max_grid_objects]
        if not w_ids:
            continue
        colored, legend = place_window_meshes(
            w_ids,
            meshes,
            id_tags,
            palette,
            view=args.view,
            layout="grid",
            prefer_protocol=True,
            wire=False,
            max_abs=args.scene_max_abs,
        )
        if not colored:
            continue
        sb = scale_bounds
        if sb is None:
            sxs = [p[0] for t in colored for p in t[:3]]
            sys_ = [p[1] for t in colored for p in t[:3]]
            sb = (min(sxs), max(sxs), min(sys_), max(sys_))
        # Auto-fit small-extent temporal windows for legibility, while still
        # recording the global scale used for the grid sheet.
        rgb, cov, meta = rasterize_scene(colored, args.strip_size, scale_bounds=None)
        idx = w["index"]
        path = out_dir / f"scene_strip_t{idx:02d}.png"
        written = write_image(path, args.strip_size, args.strip_size, rgb)
        strip_files.append(
            {
                "index": idx,
                "file": written,
                "ids": [f"0x{i:03x}" for i in w_ids],
                "events": w["event_count"],
                "step_min": w["step_min"],
                "step_max": w["step_max"],
                "coverage_px": cov,
                "tri_count": meta.get("tri_count"),
                "layout": "no_scene_matrix",
                "color_policy": "protocol_tag_or_muted",
                "legend": legend,
            }
        )
        label = (
            f"t{idx:02d} n={len(w_ids)} {w_ids[0]:03x}.."
            f"{w_ids[-1]:03x}"
        )
        strip_cells.append((label, rgb, args.strip_size))
        print(f"  strip t{idx:02d}: ids={len(w_ids)} cov={cov} -> {written}")

        if idx == 0:
            colored_w, _ = place_window_meshes(
                w_ids,
                meshes,
                id_tags,
                palette,
                view=args.view,
                layout="grid",
                prefer_protocol=True,
                wire=True,
                max_abs=args.scene_max_abs,
            )
            strip_wire_rgb, cov_rw, _ = rasterize_wire(
                colored_w, args.strip_size, scale_bounds=None
            )
            strip_wire_path = write_image(
                out_dir / "scene_strip_wire_t00.png",
                args.strip_size,
                args.strip_size,
                strip_wire_rgb,
            )
            print(f"  strip wire t00: cov={cov_rw} -> {strip_wire_path}")

    # Extra compare for first temporal window if available.
    if strip_cells and strip_wire_rgb:
        draw_side_by_side(
            out_dir / "scene_compare_t00_color_vs_wire",
            strip_cells[0][1],
            strip_wire_rgb,
            args.strip_size,
            left_label=f"{strip_cells[0][0]} protocol_tag",
            right_label="t00 wire/shaded",
        )

    # --- Contact sheet ---
    contact_cells: list[tuple[str, bytes, int]] = []
    if grid_rgb:
        contact_cells.append(("grid ids protocol_tag", grid_rgb, args.grid_size))
    contact_cells.extend(strip_cells)
    if compare_path and grid_rgb and grid_wire_rgb:
        # We already have both panels; contact will show them via files list.
        pass
    contact_path = draw_contact(
        out_dir / "scene_contact",
        contact_cells,
        cols=3,
        cell=args.contact_cell,
    )
    print(f"contact: {contact_path}")

    # --- Report ---
    protocol_ids = [
        row["id"] for row in tl_doc["id_color_table"] if row["confidence"] == "protocol_tag"
    ]
    muted_ids = [
        row["id"]
        for row in tl_doc["id_color_table"]
        if row["confidence"] != "protocol_tag"
    ]
    report = {
        "tool": "render_phase5_scene_v0377",
        "phase": "P3/P4 attract phase5 multi-object scene",
        "version": "v0377",
        "disclaimer": (
            "Host analysis reconstruction from measured submits + FIFO protocol "
            "tags. NOT recovered C. NOT CRT. Unnamed. no_scene_matrix."
        ),
        "fail_closed": {
            "logo_named": False,
            "meshes_named": False,
            "crt_claim": False,
            "scene_matrix": "absent",
            "layout_label": "no_scene_matrix",
            "color_confidence": "protocol_tag",
            "game_accurate_palette": False,
        },
        "view": args.view,
        "decode": args.decode,
        "timeline_json": str(timeline_json),
        "timeline_summary": {
            k: tl_summary.get(k)
            for k in (
                "object_events",
                "table_reads",
                "submits",
                "fifo_tags",
                "unique_id_count",
                "step_min",
                "step_max",
            )
        },
        "first_submit_id_order": tl_doc["first_submit_id_order"],
        "color_tag_table": tl_doc["color_tag_table"],
        "id_color_counts": {
            "protocol_tag": len(protocol_ids),
            "muted_default_uncorrelated": len(muted_ids),
        },
        "decoded_ok": [f"0x{i:03x}" for i in mesh_ok],
        "decoded_fail": [
            f"0x{i:03x}"
            for i in id_order
            if not meshes.get(i)
        ],
        "outputs": {
            "phase5_timeline_json": str(timeline_json),
            "scene_grid_ids": grid_path,
            "scene_grid_ids_wire": grid_wire_path,
            "scene_compare_color_vs_wire": compare_path,
            "scene_strips": [s["file"] for s in strip_files],
            "scene_strip_wire_t00": strip_wire_path,
            "scene_contact": contact_path,
        },
        "temporal_strips": strip_files,
        "grid_legend": legend_grid,
        "legibility_vs_single_object": {
            "before": (
                "single-object dumps: one mesh/PNG, tiny skip3 meshes near-blank, "
                "no co-occurrence, no protocol-tag identity"
            ),
            "after": (
                "multi-object grid + temporal strips show measured submit "
                "co-occurrence under one analysis camera; protocol-tag fills "
                "distinguish correlated ids; wire panel shows colors are "
                "differentiating but not game-accurate"
            ),
            "assessment": (
                "Colored multi-object sheets are more legible than single-object "
                "crude dumps for family structure (0x4d1 odd family, 0xd0x, "
                "0xf68-0xf76, attract 0x140/0x088). Protocol-tag colors help "
                "identity when correlated; uncorrelated muted gray is honest. "
                "Wire/shaded remains useful for silhouette."
            ),
        },
        "git_note": "out/ is gitignored; commit tools + decomp note only",
    }
    report_path = out_dir / "phase5_scene_report.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"report: {report_path}")
    print(
        f"done: strips={len(strip_files)} protocol_tag_ids={len(protocol_ids)} "
        f"muted_ids={len(muted_ids)} decoded={len(mesh_ok)}/{len(id_order)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
