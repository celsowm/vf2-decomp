#!/usr/bin/env python3
"""Rank object-table polygon meshes by visual-inspection usefulness (Phase C).

Host-side analysis of measured ROM object-table entries. Decode follows
src/hardware/tgp.c geometry_execute_object (float-link) plus a noskip
variant for stability scoring. This is evidence tooling — not recovery
semantics and not a named-mesh witness.
"""
from __future__ import annotations

import csv
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
    fill_tri,
    u32,
    write_ppm,
)

ROM_DIR = Path("roms/vf2")
OUT_DIR = Path("out/attr-render")
RANK_DIR = OUT_DIR / "rank"
NOTE_PATH = Path("decomp/i960/notes/object_rank_v0375.md")

# Measured families (ids only — no mesh semantic names).
ATTRACT_FAMILY = set(range(0x140, 0x158)) | {0x88}
POL_TEST = set(range(0x97D, 0x987))
# display_command_emit halfword table 0x70cbc (measured dump); nearby halfwords flagged too.
DISPLAY_70CBC = {0x0EE1, 0x0EED, 0x0EE3, 0x0EFD, 0x0EFF, 0x13FC, 0x13FE, 0x1400, 0x1401}
DISPLAY_NEAR = set(range(0x0EE0, 0x0F01))

ID_LO = 0x000
ID_HI = 0x1000
MAX_LINKS = 1024
POLY_ROM_BIT = 0x00800000
SCAN_EXTENT_HI = 1e3
SCAN_EXTENT_LO = 1e-3
FIN_OK = 0.9
DEGEN_TRIS = 2

try:
    from PIL import Image  # type: ignore

    HAVE_PIL = True
except Exception:
    HAVE_PIL = False


def flags_for(obj_id: int) -> list[str]:
    flags: list[str] = []
    if obj_id in ATTRACT_FAMILY:
        flags.append("attract_family")
    if obj_id in POL_TEST:
        flags.append("pol_test")
    if obj_id in DISPLAY_70CBC or obj_id in DISPLAY_NEAR:
        flags.append("display_70cbc")
    return flags


def cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def tri_area(t) -> float:
    e1 = sub(t[1], t[0])
    e2 = sub(t[2], t[0])
    c = cross(e1, e2)
    return 0.5 * math.sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2])


def pt_ok(p, lim=1e6) -> bool:
    return all(math.isfinite(c) and abs(c) < lim for c in p)


def decode_link(poly: bytes, word_index: int, skip3: bool, max_links: int = MAX_LINKS):
    """Float-link decode. skip3=True matches tgp.c when geometry_mode&3 < 2."""
    tris = []
    nonfinite = 0
    coords_seen = 0
    links = 0
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
        return tris, 0, 0, 0
    for _ in range(max_links):
        try:
            attr = rd()
        except EOFError:
            break
        if (attr & 3) == 0:
            break
        try:
            if skip3:
                rd()
                rd()
                rd()
            p2 = (f32(rd()), f32(rd()), f32(rd()))
            p3 = (f32(rd()), f32(rd()), f32(rd())) if (attr & 1) else None
        except EOFError:
            break
        links += 1
        pts = p0 + p1 + p2 + (p3 or ())
        for c in pts:
            coords_seen += 1
            if not math.isfinite(c):
                nonfinite += 1
        if all(pt_ok(p) for p in (p0, p1, p2) + ((p3,) if p3 else ())):
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
    return tris, nonfinite, coords_seen, links


def mesh_metrics(tris) -> dict:
    if not tris:
        return {
            "tris": 0,
            "bounds": None,
            "extent": 0.0,
            "area": 0.0,
            "finite_ratio": 0.0,
            "nonfinite": 0,
            "coords": 0,
        }
    xs, ys, zs = [], [], []
    area = 0.0
    for t in tris:
        for p in t:
            xs.append(p[0])
            ys.append(p[1])
            zs.append(p[2])
        try:
            area += tri_area(t)
        except Exception:
            pass
    minx, maxx = min(xs), max(xs)
    miny, maxy = min(ys), max(ys)
    minz, maxz = min(zs), max(zs)
    extent = max(maxx - minx, maxy - miny, maxz - minz)
    return {
        "tris": len(tris),
        "bounds": [minx, maxx, miny, maxy, minz, maxz],
        "extent": extent,
        "area": area,
        "finite_ratio": 1.0,
        "nonfinite": 0,
        "coords": len(xs) * 3,
    }


def stability(ta: int, tb: int) -> float:
    m = max(ta, tb)
    if m <= 0:
        return 0.0
    return max(0.0, 1.0 - abs(ta - tb) / m)


def mode_class(tris_a: int, tris_b: int, stab: float) -> str:
    """Classify skip3 vs noskip agreement (host diagnostic, not recovery)."""
    if tris_a == 0 and tris_b == 0:
        return "empty"
    if max(tris_a, tris_b) <= DEGEN_TRIS:
        return "calib_tiny"
    if tris_a >= 10 and tris_b <= max(3, int(tris_a * 0.1)):
        return "skip3_dominant"
    if tris_b >= 10 and tris_a <= max(3, int(tris_b * 0.1)):
        return "noskip_dominant"
    if tris_a >= 10 and tris_b >= 10 and stab < 0.5:
        return "both_dense_disagree"
    if stab >= 0.85:
        return "both_agree"
    return "partial_agree"


def score_entry(rec: dict) -> float:
    """Weighted usefulness for visual inspection.

    Primary geometry path is skip3 (tgp.c geometry_execute_object when
    geometry_mode&3 < 2). Stability is a soft bonus: skip3-dense meshes
    that noskip collapses are still high-value inspection targets.
    """
    if rec.get("flag_degenerate"):
        return 0.0
    tris_primary = rec.get("tris_primary", rec["tris_skip"])
    tris_a = rec["tris_skip"]
    tris_b = rec["tris_noskip"]
    stab = rec["stability"]
    fr = rec["finite_ratio"]
    ext = rec["extent"]
    area = rec["area"]
    w3 = rec["w3"]
    flags = set(rec.get("flags") or [])
    mclass = rec.get("mode_class", "")

    # Hard quality demotion for unusable inspection targets.
    if fr < 0.3:
        return 0.0
    if not (SCAN_EXTENT_LO < ext < SCAN_EXTENT_HI) and tris_primary < 20:
        return 0.0

    s = min(tris_primary, 600) / 600.0 * 0.45
    s += stab * 0.10  # soft: agreement helps, absence does not zero the mesh
    if mclass == "both_agree" and max(tris_a, tris_b) >= 8:
        s += 0.08
    if mclass == "skip3_dominant":
        s += 0.06  # dense under the measured tgp.c path
    if fr > FIN_OK:
        s += 0.15
    else:
        s += max(fr, 0.0) * 0.05
    if SCAN_EXTENT_LO < ext < SCAN_EXTENT_HI:
        s += 0.10
    elif ext > 0 and tris_primary >= 20:
        s += 0.03  # dense but extent outlier — keep visible, not top
    if w3 != 0:
        s += 0.05
    if rec.get("polygon_rom"):
        s += 0.05
    if area > 0:
        s += min(area, 200.0) / 200.0 * 0.04
    if flags & {"attract_family", "pol_test", "display_70cbc"}:
        s += 0.05
    return s


def why_row(rec: dict) -> str:
    bits = []
    if rec["flag_degenerate"]:
        bits.append("calib/prims (<=2 tris)")
    tp = rec.get("tris_primary", rec["tris_skip"])
    if tp >= 50:
        bits.append(f"alta densidade ({tp} tris)")
    elif tp >= 8:
        bits.append(f"malha média ({tp} tris)")
    elif tp >= 3:
        bits.append(f"malha pequena ({tp} tris)")
    mclass = rec.get("mode_class", "")
    if mclass == "both_agree":
        bits.append("skip3≈noskip")
    elif mclass == "skip3_dominant":
        bits.append(
            f"skip3-dominante ({rec['tris_skip']}/{rec['tris_noskip']}; "
            "caminho tgp.c)"
        )
    elif mclass == "noskip_dominant":
        bits.append(f"noskip-dominante ({rec['tris_skip']}/{rec['tris_noskip']})")
    elif mclass == "both_dense_disagree":
        bits.append(
            f"ambos densos mas divergem ({rec['tris_skip']}/{rec['tris_noskip']})"
        )
    elif rec["stability"] < 0.5 and max(rec["tris_skip"], rec["tris_noskip"]) > 0:
        bits.append(f"modos divergem ({rec['tris_skip']}/{rec['tris_noskip']})")
    if rec["finite_ratio"] > FIN_OK:
        bits.append("coords finitas")
    elif rec["finite_ratio"] < 0.5:
        bits.append("muitas coords não-finitas")
    if SCAN_EXTENT_LO < rec["extent"] < SCAN_EXTENT_HI:
        bits.append(f"extent={rec['extent']:.4g} (utilizável)")
    elif rec["extent"] > 0:
        bits.append(f"extent={rec['extent']:.4g} (fora da faixa)")
    if rec["w3"] != 0:
        bits.append(f"w3={rec['w3']:#010x}")
    if rec.get("polygon_rom"):
        bits.append("polygon-ROM")
    for f in rec.get("flags") or []:
        bits.append(f)
    if not rec.get("polygon_rom") and rec.get("poly_index"):
        bits.append("fonte não-ROM (índice RAM/hipótese)")
    return "; ".join(bits) if bits else "sem evidência de malha"


def save_image(path_base: Path, w: int, h: int, buf: list[int]) -> list[str]:
    paths = []
    ppm = path_base.with_suffix(".ppm")
    write_ppm(ppm, w, h, buf)
    paths.append(str(ppm))
    if HAVE_PIL:
        png = path_base.with_suffix(".png")
        img = Image.new("RGB", (w, h))
        pixels = []
        for c in buf:
            pixels.append(((c >> 16) & 255, (c >> 8) & 255, c & 255))
        img.putdata(pixels)
        img.save(png)
        paths.append(str(png))
    return paths


def project_view(tris, view: str, w: int, h: int):
    """Orthographic projection; returns screen tris with depth + bounds."""

    def xf(p):
        x, y, z = p
        if view == "front":
            return x, y, z
        if view == "top":
            return x, z, y
        if view == "iso":
            # simple host iso: (x - z, y + 0.5*(x + z), depth)
            return (x - z), (y + 0.5 * (x + z)), (x + y + z)
        return x, y, z

    if not tris:
        return [], None
    xformed = [tuple(xf(p) for p in t) for t in tris]
    pts = [p for t in xformed for p in t]
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    zs = [p[2] for p in pts]
    minx, maxx = min(xs), max(xs)
    miny, maxy = min(ys), max(ys)
    minz, maxz = min(zs), max(zs)
    sx = (w - 20) / max(maxx - minx, 1e-6)
    sy = (h - 20) / max(maxy - miny, 1e-6)
    s = min(sx, sy)

    def to_screen(p):
        x = int((p[0] - minx) * s) + 10
        y = int((maxy - p[1]) * s) + 10
        z = p[2]
        return x, y, z

    out = [tuple(to_screen(p) for p in t) for t in xformed]
    # painter: farther (larger depth for front/top z=+ into screen?) sort ascending mean z
    out.sort(key=lambda t: (t[0][2] + t[1][2] + t[2][2]) / 3.0)
    bounds = [minx, maxx, miny, maxy, minz, maxz]
    return out, bounds


def render_multiview(tris, out_base: Path, panel: int = 200) -> list[str]:
    """iso | front | top z-sorted filled strip."""
    views = ("iso", "front", "top")
    pw = panel
    ph = panel
    strip_w = pw * 3 + 16
    strip_h = ph + 8
    buf = [0] * (strip_w * strip_h)
    # dark background
    for i in range(len(buf)):
        buf[i] = 0x101018
    all_paths: list[str] = []
    for vi, view in enumerate(views):
        projected, _ = project_view(tris, view, pw, ph)
        ox = vi * (pw + 8) + 4
        oy = 4
        for i, tri in enumerate(projected):
            # shade by index; tint by view
            shade = 40 + (i * 47) % 200
            if view == "iso":
                color = (shade << 16) | ((shade // 2) << 8) | 255
            elif view == "front":
                color = ((shade // 2) << 16) | (shade << 8) | 200
            else:
                color = ((shade // 3) << 16) | ((shade // 2) << 8) | shade
            # offset into strip
            (x0, y0, z0), (x1, y1, z1), (x2, y2, z2) = tri
            shifted = (
                (x0 + ox, y0 + oy, z0),
                (x1 + ox, y1 + oy, z1),
                (x2 + ox, y2 + oy, z2),
            )
            fill_tri(buf, strip_w, strip_h, shifted, color)
    all_paths.extend(save_image(out_base, strip_w, strip_h, buf))
    return all_paths


def build_regions():
    if not ROM_DIR.is_dir():
        raise SystemExit(f"missing ROM dir: {ROM_DIR}")
    main_img = build(MAIN_PAIRS, 0x02400000)
    poly = build(POLY_PAIRS, 0x02000000)
    return main_img, poly


def scan_objects(main_img: bytes, poly: bytes) -> list[dict]:
    rows: list[dict] = []
    table_off0 = TABLE - BASE
    for obj in range(ID_LO, ID_HI):
        off = table_off0 + obj * 16
        if off + 16 > len(main_img):
            break
        w0, w1, w2, w3 = struct.unpack_from("<IIII", main_img, off)
        poly_rom = bool(w2 & POLY_ROM_BIT)
        poly_index = w2 & 0x7FFFFF
        if not poly_rom and poly_index == 0:
            continue
        flags = flags_for(obj)
        tris_a, nf_a, cs_a, lk_a = decode_link(poly, poly_index, skip3=True)
        tris_b, nf_b, cs_b, lk_b = decode_link(poly, poly_index, skip3=False)
        ma = mesh_metrics(tris_a)
        mb = mesh_metrics(tris_b)
        # Prefer skip-trace nonfinite accounting for the reported ratio.
        coords = max(cs_a, 1)
        finite_ratio = 1.0 - (nf_a / coords) if coords else 0.0
        if ma["tris"] == 0 and nf_a:
            finite_ratio = 0.0
        if ma["tris"] > 0:
            finite_ratio = max(finite_ratio, 0.0)
            # accepted tris are finite by construction; report link-stream ratio too
            finite_ratio = 1.0 - (nf_a / coords) if coords else 1.0

        stab = stability(ma["tris"], mb["tris"])
        extent = ma["extent"] if ma["extent"] > 0 else mb["extent"]
        area = max(ma["area"], mb["area"])
        tris_best = max(ma["tris"], mb["tris"])
        # Primary inspection path = skip3 (tgp.c geometry_mode&3 < 2).
        tris_primary = ma["tris"] if (poly_rom and ma["tris"] >= mb["tris"]) else tris_best
        if poly_rom and ma["tris"] >= 3:
            tris_primary = ma["tris"]
        elif not poly_rom:
            tris_primary = tris_best
        mclass = mode_class(ma["tris"], mb["tris"], stab)
        degen = tris_best <= DEGEN_TRIS
        # tiny prim family (pol_test-like) when both interpretations are tiny
        tiny_prim = tris_best <= 4 and ma["tris"] <= 2

        if tris_best == 0:
            category = "empty"
        elif degen:
            category = "calib"
        elif finite_ratio < 0.3:
            category = "noisy"
        elif not (SCAN_EXTENT_LO < extent < SCAN_EXTENT_HI) and tris_primary < 20:
            category = "extent_outlier"
        elif mclass in ("skip3_dominant", "noskip_dominant", "both_agree", "partial_agree", "both_dense_disagree"):
            # Inspection-useful even when modes diverge, if extent/finite OK
            # or the mesh is dense enough to inspect despite extent noise.
            if SCAN_EXTENT_LO < extent < SCAN_EXTENT_HI and finite_ratio >= 0.5:
                category = "art_candidate"
            elif tris_primary >= 20 and finite_ratio >= 0.5:
                category = "art_candidate_extent_flagged"
            else:
                category = "low_signal"
        else:
            category = "low_signal"

        rec = {
            "id": obj,
            "id_hex": f"0x{obj:03x}",
            "w0": w0,
            "w1": w1,
            "w2": w2,
            "w3": w3,
            "polygon_rom": poly_rom,
            "poly_index": poly_index,
            "poly_byte": poly_index * 4,
            "tris_skip": ma["tris"],
            "tris_noskip": mb["tris"],
            "tris_best": tris_best,
            "tris_primary": tris_primary,
            "mode_class": mclass,
            "links_skip": lk_a,
            "links_noskip": lk_b,
            "bounds": ma["bounds"] or mb["bounds"],
            "extent": extent,
            "area": area,
            "nonfinite_skip": nf_a,
            "nonfinite_noskip": nf_b,
            "finite_ratio": finite_ratio,
            "stability": stab,
            "flags": flags,
            "flag_degenerate": degen,
            "flag_tiny_prim": tiny_prim,
            "category": category,
        }
        rec["score"] = score_entry(rec)
        rec["why"] = why_row(rec)
        rows.append(rec)
    return rows


def write_outputs(rows: list[dict]) -> dict:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    RANK_DIR.mkdir(parents=True, exist_ok=True)

    # Unique by poly_index preference later; sort by score desc, id asc
    ranked = sorted(rows, key=lambda r: (-r["score"], r["id"]))
    art = [r for r in ranked if r["category"] in ("art_candidate", "art_candidate_extent_flagged")]
    calib = [r for r in ranked if r["category"] == "calib"]
    other = [r for r in ranked if r["category"] not in ("art_candidate", "art_candidate_extent_flagged", "calib")]

    json_path = OUT_DIR / "object_rank.json"
    csv_path = OUT_DIR / "object_rank.csv"
    md_path = OUT_DIR / "rank_top24.md"

    payload = {
        "meta": {
            "rom_dir": str(ROM_DIR),
            "table": hex(TABLE),
            "base": hex(BASE),
            "id_range": [hex(ID_LO), hex(ID_HI)],
            "max_links": MAX_LINKS,
            "decode": {
                "skip3": "tgp.c geometry_execute_object when geometry_mode&3 < 2",
                "noskip": "alternate hypothesis geometry_mode&3 >= 2",
                "primary_for_score": "skip3 when polygon-ROM",
            },
            "score_weights": {
                "tris_primary_cap600": 0.45,
                "stability_soft": 0.10,
                "both_agree_bonus": 0.08,
                "skip3_dominant_bonus": 0.06,
                "finite_ratio_gt_0.9": 0.15,
                "extent_1e-3_to_1e3": 0.10,
                "w3_nonzero": 0.05,
                "polygon_rom": 0.05,
                "area_cap200": 0.04,
                "measured_family_bonus": 0.05,
            },
            "pil_png": HAVE_PIL,
            "fail_closed": "unnamed meshes; not a logo/title witness",
            "counts": {
                "scanned_with_poly": len(rows),
                "art_candidate": len(art),
                "calib": len(calib),
                "other": len(other),
            },
        },
        "ranked": ranked,
        "art_top": art[:48],
        "calib_sample": calib[:24],
    }
    json_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    fieldnames = [
        "rank",
        "id",
        "id_hex",
        "score",
        "category",
        "tris_skip",
        "tris_noskip",
        "tris_primary",
        "mode_class",
        "stability",
        "finite_ratio",
        "extent",
        "area",
        "w3",
        "w3_hex",
        "polygon_rom",
        "poly_index",
        "poly_index_hex",
        "poly_byte",
        "flags",
        "flag_degenerate",
        "flag_tiny_prim",
        "why",
        "w0_hex",
        "w1_hex",
        "w2_hex",
    ]
    with csv_path.open("w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=fieldnames)
        w.writeheader()
        for i, r in enumerate(ranked, 1):
            w.writerow(
                {
                    "rank": i,
                    "id": r["id"],
                    "id_hex": r["id_hex"],
                    "score": f"{r['score']:.4f}",
                    "category": r["category"],
                    "tris_skip": r["tris_skip"],
                    "tris_noskip": r["tris_noskip"],
                    "tris_primary": r.get("tris_primary", r["tris_skip"]),
                    "mode_class": r.get("mode_class", ""),
                    "stability": f"{r['stability']:.4f}",
                    "finite_ratio": f"{r['finite_ratio']:.4f}",
                    "extent": f"{r['extent']:.6g}",
                    "area": f"{r['area']:.6g}",
                    "w3": r["w3"],
                    "w3_hex": f"{r['w3']:#010x}",
                    "polygon_rom": int(r["polygon_rom"]),
                    "poly_index": r["poly_index"],
                    "poly_index_hex": f"{r['poly_index']:#x}",
                    "poly_byte": f"{r['poly_byte']:#x}",
                    "flags": "|".join(r["flags"]),
                    "flag_degenerate": int(r["flag_degenerate"]),
                    "flag_tiny_prim": int(r.get("flag_tiny_prim", 0)),
                    "why": r["why"],
                    "w0_hex": f"{r['w0']:#010x}",
                    "w1_hex": f"{r['w1']:#010x}",
                    "w2_hex": f"{r['w2']:#010x}",
                }
            )

    # top24 markdown — art candidates first, then notable flagged/calib
    top_rows = art[:24]
    if len(top_rows) < 24:
        seen = {r["id"] for r in top_rows}
        for r in ranked:
            if r["id"] in seen:
                continue
            if r["flags"] or r["category"] in (
                "art_candidate_extent_flagged",
                "low_signal",
                "noisy",
                "calib",
            ):
                top_rows.append(r)
                seen.add(r["id"])
            if len(top_rows) >= 24:
                break
    top_rows = top_rows[:24]

    lines = [
        "# object_rank top 24 (host analysis)",
        "",
        "Fail-closed: malhas **sem nome**; isto **não** é witness de logo/title.",
        "Decode: skip3 = `tgp.c geometry_execute_object` (mode&3<2, caminho primário);",
        "noskip = hipótese alternária (estabilidade é sinal soft — não exclui malhas densas skip3).",
        f"Varredura id `0x{ID_LO:03x}–0x{ID_HI:03x}` com índice polygon != 0 ou bit ROM.",
        "",
        "| # | id | score | tris s/n | mode | stab | finite | extent | w3 | flags | category | why |",
        "| ---: | ---: | ---: | ---: | --- | ---: | ---: | ---: | ---: | --- | --- | --- |",
    ]
    for i, r in enumerate(top_rows, 1):
        lines.append(
            "| {i} | `{id}` | {sc:.3f} | {ta}/{tb} | {mc} | {st:.2f} | {fr:.2f} | {ex:.4g} | `{w3}` | {fl} | {cat} | {why} |".format(
                i=i,
                id=r["id_hex"],
                sc=r["score"],
                ta=r["tris_skip"],
                tb=r["tris_noskip"],
                mc=r.get("mode_class", "—"),
                st=r["stability"],
                fr=r["finite_ratio"],
                ex=r["extent"],
                w3=f"{r['w3']:#010x}",
                fl=",".join(r["flags"]) or "—",
                cat=r["category"],
                why=r["why"].replace("|", "/"),
            )
        )
    lines.append("")
    lines.append("## Contagens")
    lines.append("")
    lines.append(f"- scanned_with_poly: **{len(rows)}**")
    lines.append(f"- art_candidate: **{len(art)}**")
    lines.append(f"- calib (<=2 tris): **{len(calib)}**")
    lines.append(f"- other: **{len(other)}**")
    lines.append("")
    lines.append("## Artefatos")
    lines.append("")
    lines.append("- `out/attr-render/object_rank.json`")
    lines.append("- `out/attr-render/object_rank.csv`")
    lines.append("- `out/attr-render/rank/` (multi-view iso|front|top)")
    lines.append("")
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return {"ranked": ranked, "art": art, "json": json_path, "csv": csv_path, "md": md_path}


def render_top(art: list[dict], poly: bytes, limit: int = 12) -> list[dict]:
    """Render top inspection ids (prefer skip3 / tris_primary)."""
    RANK_DIR.mkdir(parents=True, exist_ok=True)
    chosen: list[dict] = []
    seen_words: set[int] = set()
    # Prefer high primary tris, good finite/extent; allow flagged families.
    pool = sorted(
        art,
        key=lambda r: (
            -r["score"],
            -r.get("tris_primary", r["tris_skip"]),
            r["id"],
        ),
    )
    for r in pool:
        if len(chosen) >= limit:
            break
        tp = r.get("tris_primary", r["tris_skip"])
        if tp < 3 and not r["flags"]:
            continue
        if r["poly_index"] in seen_words and not r["flags"]:
            continue
        tris, _, _, _ = decode_link(poly, r["poly_index"], skip3=True)
        if len(tris) < r["tris_noskip"]:
            tris_ns, _, _, _ = decode_link(poly, r["poly_index"], skip3=False)
            if len(tris_ns) > len(tris):
                tris = tris_ns
        if not tris:
            continue
        flag_tag = ("_" + "_".join(r["flags"][:2])) if r["flags"] else ""
        base = RANK_DIR / f"id{r['id']:03x}_w{r['poly_index']:x}_t{len(tris)}{flag_tag}"
        paths = render_multiview(tris, base)
        chosen.append(
            {
                "id": r["id"],
                "id_hex": r["id_hex"],
                "poly_index": r["poly_index"],
                "tris_rendered": len(tris),
                "tris_skip": r["tris_skip"],
                "tris_noskip": r["tris_noskip"],
                "score": r["score"],
                "stability": r["stability"],
                "mode_class": r.get("mode_class"),
                "flags": r["flags"],
                "images": paths,
                "category": r["category"],
            }
        )
        seen_words.add(r["poly_index"])
        print(f"  rendered id={r['id_hex']} tris={len(tris)} -> {paths[0]}")
    return chosen


def write_note(rows: list[dict], art: list[dict], rendered: list[dict], paths: dict) -> None:
    NOTE_PATH.parent.mkdir(parents=True, exist_ok=True)
    art_sorted = sorted(art, key=lambda r: (-r["score"], r["id"]))[:16]
    calib = [r for r in rows if r["category"] == "calib"]
    flagged = [r for r in rows if r["flags"]]
    mode_counts: dict[str, int] = {}
    for r in rows:
        mode_counts[r.get("mode_class", "?")] = mode_counts.get(r.get("mode_class", "?"), 0) + 1
    skip3_dense = [
        r
        for r in rows
        if r.get("mode_class") == "skip3_dominant" and r["tris_skip"] >= 20
    ]
    both_agree = [r for r in rows if r.get("mode_class") == "both_agree" and r["tris_skip"] >= 8]
    both_disagree = [
        r for r in rows if r.get("mode_class") == "both_dense_disagree"
    ]

    lines = [
        "# object_rank v0375 — ranking host de meshes polygon (Phase C)",
        "",
        "## 1. Objetivo e fail-closed",
        "",
        "Rankear entradas da tabela de objetos (`0x020e0004`, 16 B/record) por",
        "utilidade para **inspeção visual**, com métricas medidas de decode host.",
        "",
        "- **Não** é recovery de semântica de jogo.",
        "- **Não** é witness nomeado de logo/title/SEGA — meshes permanecem **sem nome**.",
        "- Projeção host é ortográfica simples; matrix/câmera do jogo não aplicadas",
        "  (exceto quando `render_mesh_host.py` recebe matrix medida via CLI).",
        "",
        "## 2. Metodologia",
        "",
        "1. Reconstruir `main_img` + `poly` a partir de `MAIN_PAIRS`/`POLY_PAIRS`",
        "   (`tools/python/render_poly_objects.py`), ROM dir `roms/vf2`.",
        "2. Varredura ids `0x000–0xFFF` onde `w2` tem bit polygon-ROM `0x00800000`",
        "   ou índice poly != 0.",
        "3. Decode float-link em dois modos:",
        "   - **skip3**: alinhado a `src/hardware/tgp.c` `geometry_execute_object`",
        "     (quando `geometry_mode & 3 < 2` descarta 3 words antes de p2) —",
        "     **caminho primário de score** para polygon-ROM;",
        "   - **noskip**: hipótese `geometry_mode & 3 >= 2` (diagnóstico).",
        "4. Métricas por id: `tris_skip`, `tris_noskip`, `tris_primary`,",
        "   `mode_class`, `stability = 1 - |ta-tb|/max`, bounds/extent,",
        "   `finite_ratio` (stream), área (soma ½|e1×e2|), pack `w3`.",
        "5. `mode_class` (host diagnostic):",
        "   - `skip3_dominant` — skip3 denso, noskip colapsa (comum em poly-ROM);",
        "   - `noskip_dominant` — o oposto;",
        "   - `both_agree` — contagens altas e próximas;",
        "   - `both_dense_disagree` — ambos densos mas divergem.",
        "6. Score: prioriza **tris_primary (skip3)** alto; estabilidade é bônus",
        "   *soft* (não zera malhas densas skip3); exige `finite_ratio` e extent",
        "   utilizáveis; `w3!=0` e bit ROM somam; bônus se id em família medida.",
        "7. Degenerados (0–2 tris) → **calib**, score 0 — não são arte.",
        "8. Top art: `render_mesh_host.py --id …` quando presente; strips",
        "   multi-view embutidos (iso|front|top) em `out/attr-render/rank/`.",
        "",
        "## 3. Contagens medidas",
        "",
        f"- scanned_with_poly: **{len(rows)}**",
        f"- art_candidate(+extent_flagged): **{len(art)}**",
        f"- calib (<=2 tris): **{len(calib)}**",
        f"- flagged (attract/pol_test/display_70cbc): **{len(flagged)}**",
        f"- skip3_dominant com tris>=20: **{len(skip3_dense)}**",
        f"- both_agree com tris>=8: **{len(both_agree)}**",
        f"- both_dense_disagree: **{len(both_disagree)}**",
        f"- Pillow PNG: **{HAVE_PIL}**",
        "",
        "## 4. Top art (por score)",
        "",
        "| id | score | tris s/n | mode | stab | extent | w3 | flags | category |",
        "| ---: | ---: | ---: | --- | ---: | ---: | ---: | --- | --- |",
    ]
    for r in art_sorted:
        lines.append(
            "| `{id}` | {sc:.3f} | {ta}/{tb} | {mc} | {st:.2f} | {ex:.4g} | `{w3}` | {fl} | {cat} |".format(
                id=r["id_hex"],
                sc=r["score"],
                ta=r["tris_skip"],
                tb=r["tris_noskip"],
                mc=r.get("mode_class", "—"),
                st=r["stability"],
                ex=r["extent"],
                w3=f"{r['w3']:#010x}",
                fl=",".join(r["flags"]) or "—",
                cat=r["category"],
            )
        )

    lines += [
        "",
        "## 5. Famílias medidas (ids, não semântica)",
        "",
        "- `attract_family`: `0x88`, `0x140–0x157` (phase5 object-table reads)",
        "- `pol_test`: `0x97d–0x986` (primitives pequenos; calib)",
        "- `display_70cbc`: `0xee1` e vizinhos/halfwords medidos (`0x0eed`, `0x13fc`…)",
        "",
        "## 6. Estabilidade de decode (mode_class)",
        "",
    ]
    for k in sorted(mode_counts, key=lambda x: -mode_counts[x]):
        lines.append(f"- mode_class `{k}`: **{mode_counts[k]}**")

    if skip3_dense:
        lines += ["", "### Densas sob skip3 (caminho tgp.c) — candidatas a inspeção", ""]
        for r in sorted(skip3_dense, key=lambda x: -x["tris_skip"])[:12]:
            lines.append(
                f"- `{r['id_hex']}` skip3={r['tris_skip']} noskip={r['tris_noskip']} "
                f"ext={r['extent']:.4g} score={r['score']:.3f} flags={r['flags']}"
            )

    if both_disagree:
        lines += ["", "### Ambos os modos densos mas divergem (não promover sem novo evidence)", ""]
        for r in sorted(both_disagree, key=lambda x: -x["tris_skip"])[:8]:
            lines.append(
                f"- `{r['id_hex']}` skip3={r['tris_skip']} noskip={r['tris_noskip']} "
                f"stab={r['stability']:.2f}"
            )

    lines += [
        "",
        "## 7. Top findings (não-nomeados)",
        "",
    ]
    for r in art_sorted[:8]:
        lines.append(f"- `{r['id_hex']}`: score={r['score']:.3f}; {r['why']}")
    lines += ["", "## 8. Calib / prims (não-arte)", ""]
    for r in sorted(calib, key=lambda x: x["id"])[:10]:
        lines.append(
            f"- `{r['id_hex']}` tris={r['tris_skip']}/{r['tris_noskip']} "
            f"w3={r['w3']:#010x} flags={r['flags']}"
        )

    lines += [
        "",
        "## 9. Renders produzidos",
        "",
    ]
    for e in rendered[:20]:
        imgs = e.get("images") or []
        best = e.get("best_file") or (imgs[0] if imgs else "—")
        lines.append(
            f"- `{e['id_hex']}` tool={e.get('tool','?')} tris={e.get('tris_rendered')} "
            f"flags={e.get('flags')} -> `{best}`"
        )

    lines += [
        "",
        "## 10. Artefatos",
        "",
        f"- `{paths['json']}`",
        f"- `{paths['csv']}`",
        f"- `{paths['md']}`",
        "- `out/attr-render/rank/` (PNG/PPM multi-view + render_mesh_host)",
        "- `out/attr-render/rank_manifest.json`",
        "",
        "## 11. Fronteira explícita",
        "",
        "- Rankings e PNGs são **análise host** de offsets medidos — não prova",
        "  que um id é um mesh de título/logo.",
        "- Malhas **sem nome**. `display_70cbc` / `attract_family` são rótulos de",
        "  **família de id medida**, não semântica de mesh.",
        "- `skip3_dominant` indica que o pacote se decodifica melhor no caminho",
        "  `tgp.c` (mode&3<2); **não** implica que noskip esteja errado no jogo",
        "  (geometry_mode pode variar por cena).",
        "- Sem commit; sem escrita em `src/` ou `CHANGELOG.md`.",
        "",
        "## Tools",
        "",
        "- `tools/python/rank_poly_objects.py`",
        "- `tools/python/render_poly_objects.py` (pairs + decode base)",
        "- `tools/python/render_mesh_host.py` (multi-view PNG host; paralelo)",
        "- `tools/python/scan_render_all_objects.py` (noskip reference)",
        "",
    ]
    NOTE_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {NOTE_PATH}")


def main() -> None:
    print(f"ROM={ROM_DIR} PIL={HAVE_PIL}")
    main_img, poly = build_regions()
    print(f"main_img={len(main_img):#x} poly={len(poly):#x}")
    rows = scan_objects(main_img, poly)
    print(f"scanned_with_poly={len(rows)}")
    out = write_outputs(rows)
    ranked = out["ranked"]
    art = out["art"]
    print(f"art_candidate={len(art)} calib={sum(1 for r in rows if r['category']=='calib')}")
    print("top10 art:")
    for r in art[:10]:
        print(
            f"  {r['id_hex']} score={r['score']:.3f} tris={r['tris_skip']}/{r['tris_noskip']} "
            f"mode={r.get('mode_class')} stab={r['stability']:.2f} ext={r['extent']:.4g} "
            f"flags={r['flags']}"
        )

    # Top-12 for external/built-in render: highest score art with usable mesh.
    top12 = [
        r
        for r in art
        if r.get("tris_primary", r["tris_skip"]) >= 3 or r["flags"]
    ][:12]
    top_ids = [r["id"] for r in top12]
    print("top12 render ids:", [f"0x{i:03x}" for i in top_ids])

    external = Path("tools/python/render_mesh_host.py")
    rendered: list[dict] = []
    external_ok = False
    if external.exists():
        import subprocess

        id_arg = ",".join(f"{i:#x}" for i in top_ids)
        cmd = [
            sys.executable,
            str(external),
            "--id",
            id_arg,
            "--out",
            str(RANK_DIR),
            "--size",
            "384",
            "--cell",
            "96",
            "--decode",
            "both",
            "--painter",
            "--rom-dir",
            str(ROM_DIR),
        ]
        print("external:", " ".join(cmd))
        try:
            proc = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
            if proc.stdout:
                print(proc.stdout[-3000:])
            if proc.stderr:
                print("stderr:", proc.stderr[-1500:])
            if proc.returncode == 0:
                external_ok = True
                # harvest render_report.json
                rep = RANK_DIR / "render_report.json"
                if rep.exists():
                    data = json.loads(rep.read_text(encoding="utf-8"))
                    for obj in data.get("objects", []):
                        files = obj.get("files") or []
                        if obj.get("best_file") and obj["best_file"] not in files:
                            files = [obj["best_file"]] + files
                        try:
                            oid = int(str(obj.get("id", "0")).replace("0x", ""), 16)
                        except Exception:
                            continue
                        rec = next((r for r in rows if r["id"] == oid), {})
                        rendered.append(
                            {
                                "id": oid,
                                "id_hex": f"0x{oid:03x}",
                                "poly_index": rec.get("poly_index"),
                                "tris_rendered": obj.get("tris_skip3")
                                or obj.get("tris_noskip"),
                                "tris_skip": obj.get("tris_skip3"),
                                "tris_noskip": obj.get("tris_noskip"),
                                "score": rec.get("score"),
                                "stability": rec.get("stability"),
                                "mode_class": rec.get("mode_class"),
                                "flags": rec.get("flags") or [],
                                "images": files,
                                "best_file": obj.get("best_file"),
                                "category": rec.get("category"),
                                "tool": "render_mesh_host",
                            }
                        )
                print(f"external render ok: {len(rendered)} objects")
            else:
                raise RuntimeError(f"external rc={proc.returncode}")
        except Exception as e:
            print(f"external render failed ({e}); fallback built-in multi-view")
            rendered = render_top(art, poly, limit=12)
    else:
        print("render_mesh_host.py absent — built-in multi-view")
        rendered = render_top(art, poly, limit=12)

    # Built-in multi-view also for top12 + flagged specials (extra comparison strip).
    builtin_extra = render_top(top12 if top12 else art, poly, limit=12)
    poly_map = {r["id"]: r for r in rows}
    for want in (0x88, 0x148, 0x97D, 0xEE1):
        rec = poly_map.get(want)
        if not rec:
            continue
        tris, _, _, _ = decode_link(poly, rec["poly_index"], skip3=True)
        if len(tris) < rec.get("tris_noskip", 0):
            tris_ns, _, _, _ = decode_link(poly, rec["poly_index"], skip3=False)
            if len(tris_ns) > len(tris):
                tris = tris_ns
        if not tris:
            print(f"  special {want:#x}: no tris")
            continue
        flag_tag = ("_" + "_".join(rec["flags"][:2])) if rec["flags"] else ""
        base = RANK_DIR / f"special_{want:03x}_w{rec['poly_index']:x}_t{len(tris)}{flag_tag}"
        paths = render_multiview(tris, base)
        entry = {
            "id": want,
            "id_hex": f"0x{want:03x}",
            "poly_index": rec["poly_index"],
            "tris_rendered": len(tris),
            "tris_skip": rec["tris_skip"],
            "tris_noskip": rec["tris_noskip"],
            "score": rec["score"],
            "stability": rec["stability"],
            "mode_class": rec.get("mode_class"),
            "flags": rec["flags"],
            "images": paths,
            "category": rec["category"],
            "special": True,
            "tool": "rank_builtin",
        }
        rendered.append(entry)
        print(f"  rendered special id={entry['id_hex']} tris={len(tris)} -> {paths[0]}")

    # merge builtin strips that are not already recorded
    seen_ids = {e["id"] for e in rendered}
    for e in builtin_extra:
        if e["id"] not in seen_ids:
            e = dict(e)
            e["tool"] = "rank_builtin"
            rendered.append(e)
            seen_ids.add(e["id"])

    (OUT_DIR / "rank_manifest.json").write_text(
        json.dumps(
            {
                "rendered": rendered,
                "pil": HAVE_PIL,
                "external_tool": str(external) if external.exists() else None,
                "external_ok": external_ok,
                "top12_ids": [f"0x{i:03x}" for i in top_ids],
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    write_note(rows, art, rendered, out)
    print("done")


if __name__ == "__main__":
    main()
