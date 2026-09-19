#!/usr/bin/env python3
"""Score polygon-ROM packet decode hypotheses on pol_test calibration.

Evidence tooling — host-side analysis of measured ROM offsets and w3 size
hints. Not recovered game C. Fail-closed: a winning host mode is a
hypothesis until oracle-pinned at a geometry boundary.

Decode modes (names stable for render_mesh_host --decode):
  skip3_float_link     tgp.c geometry_execute_object, mode&3<2 (skip 3)
  noskip_float_link    same, skip 0 words after attr
  skip2_float_link     skip 2 words after attr
  skip4_float_link     skip 4 words after attr
  skip3_quad_bit2      attr&2 selects quad (instead of attr&1)
  skip3_end_attr0      end when attr==0 (not attr&3==0)
  skip3_fixed_link     ignore attr>>8; always p0=p2, p1=p3||p2
  noskip_int16         no header floats; s16 xyz pairs after attr, skip 0
  skip3_int16          s16 xyz after attr, skip 3
  skip3_no_header      no 6-float p0/p1 header; first word is attr

Scoring (measured features only):
  finite_ratio, tri_count vs w3 halfwords, bounds vs w3 pair,
  edge-length spikes, cross-id consistency on pol_test 0x97d-0x986.
"""
from __future__ import annotations

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

OUT = Path("out/attr-p1")
POL_TEST = list(range(0x97D, 0x987))
CALIB_IDS = POL_TEST + [0x148, 0x14F, 0x150, 0x1CB, 0x5C7]
EDGE_SPIKE = 1e5


def s16(word: int) -> int:
    v = word & 0xFFFF
    return v - 0x10000 if v >= 0x8000 else v


def decode_mode(poly: bytes, word_index: int, mode: str, max_links: int = 4096):
    """Return (tris, meta). Each tri is 3 (x,y,z) tuples."""
    tris = []
    meta = {
        "mode": mode,
        "links": 0,
        "nonfinite": 0,
        "eof": False,
        "end_reason": "",
        "attrs": [],
        "verts_sample": [],
    }
    off = word_index * 4

    def rd() -> int:
        nonlocal off
        if off < 0 or off + 4 > len(poly):
            raise EOFError
        v = u32(poly, off)
        off += 4
        return v

    def ftrip() -> tuple:
        return (f32(rd()), f32(rd()), f32(rd()))

    def i16trip() -> tuple:
        return float(s16(rd())), float(s16(rd())), float(s16(rd()))

    try:
        if mode in (
            "noskip_int16",
            "skip3_no_header",
            "skip3_quad_no_header",
            "noskip_quad_no_header",
            "skip3_tri_no_header",
        ):
            p0 = p1 = (0.0, 0.0, 0.0)
        else:
            p0 = ftrip()
            p1 = ftrip()
    except EOFError:
        meta["eof"] = True
        meta["end_reason"] = "header_eof"
        return tris, meta

    skip_words = {
        "skip3_float_link": 3,
        "skip3_quad_bit2": 3,
        "skip3_end_attr0": 3,
        "skip3_fixed_link": 3,
        "skip3_int16": 3,
        "skip3_no_header": 3,
        "skip3_quad_no_header": 3,
        "noskip_quad_no_header": 0,
        "skip3_tri_no_header": 3,
        "noskip_float_link": 0,
        "noskip_int16": 0,
        "skip2_float_link": 2,
        "skip4_float_link": 4,
    }.get(mode, 3)

    vertex_reader = i16trip if mode.endswith("int16") else ftrip
    if mode == "skip3_quad_bit2":
        quad_sel = lambda a: (a & 2) != 0
    elif mode in ("skip3_quad_no_header", "noskip_quad_no_header"):
        quad_sel = lambda a: True  # every link is a quad (attr ignored for count)
    elif mode == "skip3_tri_no_header":
        quad_sel = lambda a: False  # force tris even if attr&1
    else:
        quad_sel = lambda a: (a & 1) != 0
    end_test = (lambda a: a == 0) if mode == "skip3_end_attr0" else (
        lambda a: (a & 3) == 0
    )

    for _ in range(max_links):
        try:
            attr = rd()
        except EOFError:
            meta["eof"] = True
            meta["end_reason"] = "attr_eof"
            break
        if end_test(attr):
            meta["end_reason"] = "end_marker"
            break
        meta["attrs"].append(attr)
        try:
            for _k in range(skip_words):
                rd()
            p2 = vertex_reader()
            p3 = vertex_reader() if quad_sel(attr) else None
        except EOFError:
            meta["eof"] = True
            meta["end_reason"] = "body_eof"
            break
        meta["links"] += 1
        pts = p0 + p1 + p2 + (p3 or ())
        ok = all(math.isfinite(c) and abs(c) < 1e6 for c in pts)
        if ok:
            tris.append((p0, p1, p2))
            if p3 is not None:
                tris.append((p0, p2, p3))
            if len(meta["verts_sample"]) < 8:
                meta["verts_sample"].append([list(p0), list(p1), list(p2)])
        else:
            meta["nonfinite"] += 1
        if mode == "skip3_fixed_link":
            p0, p1 = p2, (p3 if p3 is not None else p2)
        else:
            mode_bits = (attr >> 8) & 3
            if mode_bits in (0, 2):
                p0 = p2
                p1 = p3 if p3 is not None else p2
            elif mode_bits == 1:
                p1 = p2
            else:
                p0 = p3 if p3 is not None else p2
        if len(tris) > 8000:
            meta["end_reason"] = "tris_cap"
            break
    return tris, meta


def w3_halves(w3: int) -> tuple[int, int]:
    return (w3 >> 16) & 0xFFFF, w3 & 0xFFFF


def mesh_stats(tris) -> dict:
    if not tris:
        return {
            "tris": 0,
            "finite_ratio": 0.0,
            "extent": 0.0,
            "bounds": None,
            "edges_max": 0.0,
            "spikes": 0,
            "degen": 0,
        }
    xs, ys, zs = [], [], []
    edges_max = 0.0
    spikes = 0
    degen = 0
    for t in tris:
        for p in t:
            xs.append(p[0])
            ys.append(p[1])
            zs.append(p[2])
        for a, b in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
            e = math.sqrt(
                (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2
            )
            if e > edges_max:
                edges_max = e
            if e > EDGE_SPIKE:
                spikes += 1
        # degenerate if all three nearly identical
        d01 = sum((t[0][i] - t[1][i]) ** 2 for i in range(3))
        d12 = sum((t[1][i] - t[2][i]) ** 2 for i in range(3))
        if d01 < 1e-12 and d12 < 1e-12:
            degen += 1
    minx, maxx = min(xs), max(xs)
    miny, maxy = min(ys), max(ys)
    minz, maxz = min(zs), max(zs)
    extent = max(maxx - minx, maxy - miny, maxz - minz)
    n_coords = len(xs) * 3
    finite = sum(
        1 for t in tris for p in t for c in p
        if math.isfinite(c) and abs(c) < 1e6
    )
    return {
        "tris": len(tris),
        "finite_ratio": finite / n_coords if n_coords else 0.0,
        "extent": extent,
        "bounds": [minx, maxx, miny, maxy, minz, maxz],
        "edges_max": edges_max,
        "spikes": spikes,
        "degen": degen,
    }


def score_case(stats: dict, w3: int, links: int) -> dict:
    """Score one object decode. Higher is better. Components in [0,1] roughly.

    Measured pol_test calibration (oracle table + poly-ROM span deltas):
      id 0x97d w3=0x00010002 word_span=21
      id 0x97e w3=0x000a000b word_span=111
      id 0x97f w3=0x00640065 word_span=1011
    Span = 6 header + sum(prim words) + 1 end, with skip3 tris=7 / quads=10
    words. Solving gives prim_count ≈ w3 lo half (11→11, 101→101).
    Host skip3_float_link on 0x97d hex yields a coherent ±0.2 XZ quad.
    """
    hi, lo = w3_halves(w3)
    tris = stats["tris"]
    comp = {}

    # (a) finite verts
    comp["finite"] = stats["finite_ratio"]

    # (b) tri/link count vs w3 lo (measured prim-count hint for pol_test).
    # Quads emit 2 tris per link; tris emit 1. Prefer links near lo.
    expected_links = max(1, lo if lo <= 4096 else 1)
    if links == 0 and tris == 0:
        comp["tri_count"] = 0.0
    elif links > 0:
        # closeness of link count to w3 lo
        denom = max(expected_links, 4)
        comp["tri_count"] = 1.0 - abs(links - expected_links) / denom
        comp["tri_count"] = max(0.0, min(1.0, comp["tri_count"]))
        # tiny pol_test family must stay tiny
        if lo <= 16 and links > 32:
            comp["tri_count"] = 0.05
    else:
        # no link metadata; fall back to tri count vs lo (or 2*lo for quads)
        for expected in (expected_links, expected_links * 2):
            if tris == expected:
                comp["tri_count"] = 1.0
                break
        else:
            denom = max(expected_links * 2, 4)
            comp["tri_count"] = 1.0 - abs(tris - expected_links) / denom
            comp["tri_count"] = max(0.0, min(1.0, comp["tri_count"]))

    # (c) bounds vs w3 pair — if w3 halves look like extents (or small counts),
    # penalize extents that are wildly larger than max(hi,lo)*scale for tiny w3.
    extent = stats["extent"]
    max_half = max(hi, lo)
    if w3 == 0x00010002:
        # tiny prim family: coherent decode should have modest local extent
        if extent <= 0:
            comp["bounds"] = 0.0
        elif extent < 1000:
            comp["bounds"] = 1.0
        elif extent < 1e4:
            comp["bounds"] = 0.5
        else:
            comp["bounds"] = 0.0
    else:
        # attract/dense: w3 often looks like packed size pair; soft preference
        if extent <= 0:
            comp["bounds"] = 0.0
        elif max_half == 0:
            comp["bounds"] = 0.4 if extent < 1e4 else 0.0
        else:
            # ratio extent/half — prefer order-of-magnitude agreement when halves are size-like
            ratio = extent / max_half
            if 0.05 <= ratio <= 50:
                comp["bounds"] = 0.8
            elif ratio < 1e3:
                comp["bounds"] = 0.4
            else:
                comp["bounds"] = 0.0

    # (d) edge spikes
    if stats["spikes"] == 0 and tris > 0:
        comp["edges"] = 1.0
    elif stats["spikes"] <= 2:
        comp["edges"] = 0.4
    else:
        comp["edges"] = 0.0
    if stats["edges_max"] > EDGE_SPIKE:
        comp["edges"] = min(comp["edges"], 0.2)

    # end_reason soft
    # links bonus for producing any geometry
    comp["produced"] = 1.0 if tris > 0 else 0.0

    total = (
        0.30 * comp["finite"]
        + 0.30 * comp["tri_count"]
        + 0.20 * comp["bounds"]
        + 0.15 * comp["edges"]
        + 0.05 * comp["produced"]
    )
    return {"total": total, "components": comp}


def load_object_table(main_img: bytes):
    table = {}
    for obj in CALIB_IDS:
        off = (TABLE - BASE) + obj * 16
        if off + 16 > len(main_img):
            continue
        w0, w1, w2, w3 = struct.unpack_from("<IIII", main_img, off)
        word_index = w2 & 0x7FFFFF
        rom = bool(w2 & 0x00800000)
        table[obj] = {
            "w0": w0,
            "w1": w1,
            "w2": w2,
            "w3": w3,
            "word_index": word_index,
            "byte": word_index * 4,
            "poly_rom": rom,
        }
    return table


def hex_dump(poly: bytes, byte_off: int, words: int = 96) -> list[dict]:
    out = []
    for i in range(words):
        o = byte_off + i * 4
        if o + 4 > len(poly):
            break
        w = u32(poly, o)
        fv = f32(w)
        out.append(
            {
                "index": i,
                "addr": o,
                "word": w,
                "hex": f"{w:08x}",
                "f32": fv if math.isfinite(fv) else None,
            }
        )
    return out


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    main_img = build(MAIN_PAIRS, 0x02400000)
    poly = build(POLY_PAIRS, 0x02000000)
    table = load_object_table(main_img)

    modes = [
        "skip3_float_link",
        "noskip_float_link",
        "skip2_float_link",
        "skip4_float_link",
        "skip3_quad_bit2",
        "skip3_end_attr0",
        "skip3_fixed_link",
        "noskip_int16",
        "skip3_int16",
        "skip3_no_header",
        "skip3_quad_no_header",
        "noskip_quad_no_header",
        "skip3_tri_no_header",
    ]

    # Hex dumps for calibration objects
    dumps = {}
    for obj in (0x97D, 0x148):
        rec = table.get(obj)
        if not rec:
            continue
        dumps[f"{obj:#x}"] = {
            "table": {k: (hex(v) if isinstance(v, int) and k != "poly_rom" else v)
                      for k, v in rec.items()},
            "words": hex_dump(poly, rec["byte"], 96),
        }
    (OUT / "poly_rom_hex.json").write_text(json.dumps(dumps, indent=2))

    # Score every mode on every calib id
    results = {m: {"objects": {}, "aggregate": {}} for m in modes}
    for mode in modes:
        pol_tris = []
        pol_scores = []
        pol_spikes = 0
        pol_finite = []
        for obj, rec in table.items():
            tris, meta = decode_mode(poly, rec["word_index"], mode)
            stats = mesh_stats(tris)
            sc = score_case(stats, rec["w3"], meta["links"])
            results[mode]["objects"][f"{obj:#x}"] = {
                "id": obj,
                "w2": hex(rec["w2"]),
                "w3": hex(rec["w3"]),
                "word_index": hex(rec["word_index"]),
                "byte": hex(rec["byte"]),
                "stats": stats,
                "score": sc,
                "links": meta["links"],
                "nonfinite": meta["nonfinite"],
                "end_reason": meta["end_reason"],
                "attrs_head": [hex(a) for a in meta["attrs"][:8]],
            }
            if obj in POL_TEST:
                pol_tris.append(stats["tris"])
                pol_scores.append(sc["total"])
                pol_spikes += stats["spikes"]
                pol_finite.append(stats["finite_ratio"])

        # (e) consistency across pol_test family:
        # Measured w3 lo varies (2,11,101,2,2,2,11,101,2,2). A good decode
        # TRACKS that scale — it does not force equal tri counts.
        # Score = correlation-style agreement between produced links/tris and w3 lo.
        if pol_tris:
            ratios = []
            for obj, o in results[mode]["objects"].items():
                if obj not in {f"{i:#x}" for i in POL_TEST}:
                    continue
                _hi, lo = w3_halves(int(o["w3"], 16))
                produced = o["links"] if o["links"] > 0 else o["stats"]["tris"]
                if lo <= 0:
                    continue
                # ideal produced ≈ lo prims; quads may yield 2*lo tris
                r1 = produced / lo
                r2 = produced / (lo * 2) if lo else 0
                # distance to nearest ideal ratio 1.0 or 0.5 (quad→2 tris)
                dist = min(abs(r1 - 1.0), abs(r2 - 1.0), abs(r1 - 0.5))
                ratios.append(dist)
            if ratios:
                mean_dist = sum(ratios) / len(ratios)
                consist = max(0.0, 1.0 - mean_dist)
                # bonus if ALL pol_test objects produced some geometry
                if all(t > 0 for t in pol_tris):
                    consist = min(1.0, consist + 0.15)
            else:
                consist = 0.0
            mean_tris = sum(pol_tris) / len(pol_tris)
            var = sum((t - mean_tris) ** 2 for t in pol_tris) / len(pol_tris)
            tiny = all(0 < t <= 256 for t in pol_tris)
        else:
            mean_tris = 0.0
            var = 0.0
            consist = 0.0
            tiny = False

        # geometric coherence on pol_test: no spikes + finite
        pol_finite_mean = sum(pol_finite) / len(pol_finite) if pol_finite else 0.0
        spike_free = 1.0 if pol_spikes == 0 else 0.0

        # aggregate: pol_test w3-tracking + finite/spike + broader mean
        pol_mean = sum(pol_scores) / len(pol_scores) if pol_scores else 0.0
        all_scores = [o["score"]["total"] for o in results[mode]["objects"].values()]
        all_mean = sum(all_scores) / len(all_scores) if all_scores else 0.0
        dense_spikes = sum(
            o["stats"]["spikes"]
            for oid, o in results[mode]["objects"].items()
            if oid not in {f"{x:#x}" for x in POL_TEST}
        )
        aggregate = (
            0.40 * consist
            + 0.25 * pol_mean
            + 0.15 * pol_finite_mean
            + 0.10 * spike_free
            + 0.07 * all_mean
            + 0.03 * (1.0 if dense_spikes < 20 else 0.0)
        )
        results[mode]["aggregate"] = {
            "score": aggregate,
            "pol_mean_score": pol_mean,
            "pol_consistency": consist,
            "pol_tris": pol_tris,
            "pol_tris_mean": mean_tris,
            "pol_tris_var": var,
            "pol_tiny": tiny,
            "pol_spikes": pol_spikes,
            "all_mean_score": all_mean,
            "dense_spikes": dense_spikes,
        }

    ranked = sorted(
        (
            {
                "mode": m,
                "score": results[m]["aggregate"]["score"],
                **{k: v for k, v in results[m]["aggregate"].items() if k != "score"},
            }
            for m in modes
        ),
        key=lambda r: -r["score"],
    )

    # Hex-level calibration override (measured poly ROM, not just aggregate score):
    # pol_test 0x97d @ word 0x18a0b1: header p0=(-0.2,0,0.2) p1=(-0.2,0,-0.2);
    # attr word 0xe1001601 (attr&3=1, attr&1=1); skip3=(0,-1,0);
    # p2=(0.2,0,0.2) p3=(0.2,0,-0.2) → coherent ±0.2 XZ quad.
    # That matches tgp.c geometry_execute_object skip3 + attr&1 quad.
    # Automated score may prefer skip3_quad_bit2/noskip; hex evidence selects
    # skip3_float_link as the best *calibrated* host hypothesis.
    hex_preferred = "skip3_float_link"
    best_auto = ranked[0]["mode"] if ranked else None
    best_chosen = hex_preferred if hex_preferred in results else best_auto

    # Annotate rank.json with chosen mode after write fields exist.
    report = {
        "note": (
            "Host-side decode hypothesis ranking. pol_test 0x97d-0x986 are "
            "measured tiny prims (w3=0x00010002). Winning mode is a host "
            "hypothesis — not recovered C unless oracle-pinned."
        ),
        "calibration_ids_pol_test": [f"{i:#x}" for i in POL_TEST],
        "extra_ids": [f"{i:#x}" for i in (0x148, 0x14F, 0x150, 0x1CB, 0x5C7)],
        "object_table": {f"{k:#x}": {kk: (hex(vv) if isinstance(vv, int) and kk != "poly_rom" else vv) for kk, vv in v.items()} for k, v in table.items()},
        "modes": modes,
        "ranking": ranked,
        "best_mode_auto": best_auto,
        "best_mode": best_chosen,
        "hex_calibration": {
            "id": "0x97d",
            "word_index": "0x18a0b1",
            "w3": "0x00010002",
            "header": "p0=(-0.2,0,0.2) p1=(-0.2,0,-0.2)",
            "attr0": "0xe1001601 attr&3=1 attr&1=1",
            "skip3_words": "(0,-1,0) normal-like",
            "p2p3": "(0.2,0,0.2)/(0.2,0,-0.2) ±0.2 XZ quad",
            "span_words": 21,
            "span_formula": "6 header + 10 skip3-quad + 1 end + pad → next id",
            "family_w3_lo": [2, 11, 101, 2, 2, 2, 11, 101, 2, 2],
            "chosen_mode": hex_preferred,
        },
        "results": results,
        "mame_suggestions_only": {
            "model2.h:279-301": (
                "geo_process_command dispatch list: geo_object_data, "
                "geo_direct_data, geo_polygon_data, geo_mode, geo_focal_distance, "
                "geo_matrix_write, geo_translate_write, geo_end — naming only; "
                "handlers are NOT present in the vendored third_party dump."
            ),
            "model2.cpp:624-632": "copro_fifo_w: coproctl&0x80000000 program-load else FIFO push",
            "model2.cpp:822-896": (
                "geo_w packs function ((address>>4)&0x3f)<<23 into FIFO word; "
                "bit31 data is jump/tag form r=(data&0x800fffff)|func<<23"
            ),
            "model2.cpp:750": "copro_data ROM mapped at 0x00800000",
            "mb86233.cpp:20-33": "TGP RAM banks; Sega programs use IEEE float mode",
        },
        "fail_closed": (
            "Host decode mode ≠ recovered TGP geometry_execute_object unless "
            "differential/oracle write-pins the same vertex stream. "
            "P1 v0377: i960 helper/task writes are table+protocol only — "
            "poly-ROM vertex corroboration via oracle geo/FIFO is ABSENT. "
            "Auto span matches that parse IEEE floats as attr are false "
            "positives; hex-calibrated skip3_float_link is the preferred "
            "hypothesis (see out/attr-p1/oracle_decode_score.json)."
        ),
    }
    (OUT / "decode_rank.json").write_text(json.dumps(report, indent=2))
    print(f"best_mode_auto={report['best_mode_auto']}")
    print(f"best_mode_chosen={report['best_mode']} (hex-calibrated pol_test 0x97d)")
    for r in ranked:
        print(
            f"  {r['mode']:22s} score={r['score']:.4f} "
            f"pol_mean={r['pol_mean_score']:.4f} consist={r['pol_consistency']:.2f} "
            f"pol_tris={r['pol_tris']} spikes={r['pol_spikes']}"
        )
    print(f"wrote {OUT / 'decode_rank.json'}")
    print(f"wrote {OUT / 'poly_rom_hex.json'}")


if __name__ == "__main__":
    main()
