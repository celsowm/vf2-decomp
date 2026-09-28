#!/usr/bin/env python3
"""P1 oracle-vs-polyROM analysis + decode scoring (fail-closed evidence tool).

Tasks covered:
  - extract measured geo/FIFO sequences from pol_test probe JSONL
  - dump poly ROM u32 stream at each pol_test word_index
  - compare: are geo writes the same floats as poly ROM? (expect pointer only)
  - score decode modes against MEASURED span/w3 + oracle protocol counts
  - emit hypothesis table with measured|suggested_by_mame|unproven tags

Not recovered C. No invented mesh names.
"""
from __future__ import annotations

import json
import math
import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from decode_packet_hypotheses import (  # noqa: E402
    CALIB_IDS,
    POL_TEST,
    decode_mode,
    load_object_table,
    mesh_stats,
    score_case,
    w3_halves,
)
from render_poly_objects import (  # noqa: E402
    MAIN_PAIRS,
    POLY_PAIRS,
    build,
    f32,
    u32,
)

OUT = Path("out/attr-p1")
PARK = Path("out/attr-long/long-29.vf2snap")

# Pol_test task 0x21a00 measured immediates (disasm, not invented)
POL_TEST_TASK = {
    "entry": "0x21a00",
    "end_ret_path_a": "0x21b00",
    "end_ret_path_b": "0x21be8",
    "helper_submit": "0x7c60",
    "helper_palette": "0x7f24",
    "fifo_base": "g11[g12] (measured live as 0x00884000)",
    "geo_base": "g10 (measured live as 0x00800000)",
    "mode_byte": "0x00530150",
    "count_bytes": {
        "0x0053014c": "path A: 0x986 (or 0x985 if mode==3); path B: 0x97f",
        "0x0053014d": "path B: 0x97e",
        "0x0053014e": "path B: 0x97d",
    },
    "paths": {
        "path_A_mode_le_2": {
            "condition": "ldob 0x530150; cmpoble 2 → 0x21a1c",
            "pre_fifo_immediates": [
                "0x00800101",
                "0x01800303",
                "0x03000606",
                "0x3e9eb852",  # f32 ~0.3100
                "0x3e428f5c",  # f32 ~0.1900
                "0x3f9eb852",  # f32 ~1.2400
            ],
            "g0_calls": [
                "loop 0x53014c times: if 0x530150==3 then id=0x985 else id=0x986; g1=0; call 0x7c60",
                "after loop: FIFO 0x03000606, 0x00000000, 0xbec7ae14, 0x00000000",
                "g0=0x985 g1=0 call 0x7c60",
            ],
            "post_fifo": ["0x01000202"],
        },
        "path_B_mode_gt_2": {
            "condition": "else set ip continuation 0x21b04",
            "pre_fifo_immediates": ["0x00800101"],
            "g0_calls": [
                "loop 0x53014c times: g0=0x97f g1=0 call 0x7c60",
                "loop 0x53014d times: g0=0x97e g1=0 call 0x7c60",
                "loop 0x53014e times: g0=0x97d g1=0 call 0x7c60",
            ],
            "post_prim_fifo_immediates": [
                "0x01000202",
                "0x00800101",
                "0x01800303",
                "0x03000606",
                "0x3f428f5c",  # f32 ~0.7600
                "0xbf3851ec",  # f32 ~-0.7200
                "0x401f5c29",  # f32 ~2.4900
            ],
            "final_call": "g0=0x97d g1=0 call 0x7c60; FIFO 0x01000202; ret",
        },
    },
    "g1_always": 0,
    "note": (
        "Disasm is MEASURED from roms/vf2 via vf2i960 function 0x21a00. "
        "g1=0 on every pol_test call → helper skips 0x5010d0 accum path."
    ),
}

# FIFO protocol words seen around helper (measured class hints via >>23)
KNOWN_FIFO = {
    "0x00800101": "class=(>>23)&0x1f=0x01; matches tgp.c case 0x01 object-data name",
    "0x01000202": "class=0x02; matches tgp.c case 0x02 direct-data name",
    "0x01800303": "class=0x03; body unproven",
    "0x03000606": "class=0x06; body unproven",
    "0x1a003434": "class=0x14; helper 0x7c60 first FIFO word; body unproven",
    "0x0d001a1a": "color-like family; hex only",
    "0x12002424": "color-like family; hex only",
    "0x12802525": "color-like family; hex only",
}


def f32_of(word: int):
    fv = f32(word)
    return fv if math.isfinite(fv) else None


def extract_oracle_sequences(jsonl: Path) -> dict:
    """Pull ordered geo/FIFO/table words from a memory-trace JSONL."""
    geo = []
    fifo = []
    table_reads = []
    all_writes = Counter()
    for ln in jsonl.read_text(errors="replace").splitlines():
        if not ln.startswith("{"):
            continue
        try:
            j = json.loads(ln)
        except Exception:
            continue
        if j.get("type") != "memory":
            continue
        a = int(j.get("address", 0))
        kind = j.get("kind")
        try:
            bs = bytes.fromhex(j.get("bytes", "00"))
            val = int.from_bytes(bs[:4], "little")
        except Exception:
            val = 0
        if kind == "write":
            all_writes[a] += 1
            if a == 0x00800010 or 0x00804000 <= a < 0x00808000 or a == 0x00800000:
                geo.append(
                    {
                        "addr": f"{a:#010x}",
                        "val": f"{val:#010x}",
                        "f32": f32_of(val),
                        "step": j.get("step"),
                    }
                )
            if 0x00884000 <= a < 0x00888000 or a == 0x00880000:
                fifo.append(
                    {
                        "addr": f"{a:#010x}",
                        "val": f"{val:#010x}",
                        "f32": f32_of(val),
                        "step": j.get("step"),
                    }
                )
        elif kind == "read":
            if 0x020E0000 <= a < 0x02100000:
                table_reads.append(
                    {
                        "addr": f"{a:#010x}",
                        "val": f"{val:#010x}",
                        "step": j.get("step"),
                    }
                )
    return {
        "geo": geo,
        "fifo": fifo,
        "table_reads": table_reads,
        "top_writes": [(hex(a), n) for a, n in all_writes.most_common(15)],
        "counts": {"geo": len(geo), "fifo": len(fifo), "table": len(table_reads)},
    }


def dump_poly_stream(poly: bytes, word_index: int, words: int = 40) -> list[dict]:
    out = []
    base = word_index * 4
    for i in range(words):
        o = base + i * 4
        if o + 4 > len(poly):
            break
        w = u32(poly, o)
        out.append(
            {
                "i": i,
                "word": f"{w:#010x}",
                "f32": f32_of(w),
                "attr_like": (w & 3) != 0 and (w >> 28) in (0x0, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF),
            }
        )
    return out


def span_to_next(table: dict, obj: int) -> int | None:
    if obj not in table:
        return None
    wi = table[obj]["word_index"]
    best = None
    for oid, rec in table.items():
        if oid == obj:
            continue
        d = rec["word_index"] - wi
        if d > 0 and (best is None or d < best):
            best = d
    return best


def words_consumed_by_mode(poly: bytes, word_index: int, mode: str) -> dict:
    """Walk decode_mode but also report words consumed until end."""
    off0 = word_index * 4
    tris, meta = decode_mode(poly, word_index, mode)
    # approximate consumption from end_reason + links is not exact; re-walk
    # a lightweight counter using the same rules as decode_mode skip map.
    skip = {
        "skip3_float_link": 3,
        "skip3_quad_bit2": 3,
        "noskip_float_link": 0,
        "skip2_float_link": 2,
        "skip4_float_link": 4,
        "skip3_quad_no_header": 3,
        "noskip_quad_no_header": 0,
        "skip3_tri_no_header": 3,
        "skip3_end_attr0": 3,
        "skip3_fixed_link": 3,
        "skip3_int16": 3,
        "noskip_int16": 0,
        "skip3_no_header": 3,
    }.get(mode, 3)
    off = off0
    consumed = 0

    def rd():
        nonlocal off, consumed
        if off + 4 > len(poly):
            raise EOFError
        v = u32(poly, off)
        off += 4
        consumed += 4
        return v

    try:
        if mode not in (
            "noskip_int16",
            "skip3_no_header",
            "skip3_quad_no_header",
            "noskip_quad_no_header",
            "skip3_tri_no_header",
        ):
            for _ in range(6):
                rd()
        while True:
            attr = rd()
            if mode == "skip3_end_attr0":
                if attr == 0:
                    break
            elif (attr & 3) == 0:
                break
            for _k in range(skip):
                rd()
            for _c in range(3):
                rd()
            if mode == "skip3_quad_bit2":
                quad = (attr & 2) != 0
            elif mode in ("skip3_quad_no_header", "noskip_quad_no_header"):
                quad = True
            elif mode == "skip3_tri_no_header":
                quad = False
            else:
                quad = (attr & 1) != 0
            if quad:
                for _c in range(3):
                    rd()
            if consumed // 4 > 4000:
                break
    except EOFError:
        pass
    return {
        "words": consumed // 4,
        "bytes": consumed,
        "links": meta["links"],
        "tris": len(tris),
        "end_reason": meta["end_reason"],
        "attrs_head": [hex(a) for a in meta["attrs"][:6]],
        "stats": mesh_stats(tris),
    }


def score_against_oracle(
    table: dict,
    poly: bytes,
    oracle_by_id: dict,
    modes: list[str],
) -> dict:
    """Score modes using MEASURED oracle quantities only.

    Oracle corroboration available:
      - w3 lo/hi from object table (measured live + ROM)
      - poly word_index + span to next id (measured ROM)
      - helper writes table w0/w1/w2 and r11=-1, NOT vertex floats
      - FIFO protocol words do NOT encode vertex counts for pol_test
      - path A/B call counts are controlled by work-RAM bytes 0x53014c-e
    Therefore vertex-count scoring uses w3-lo + poly-ROM span, and a
    separate MATCH/MISMATCH of "decode consumption vs span".
    """
    results = {}
    for mode in modes:
        rows = {}
        span_matches = 0
        span_checked = 0
        w3_link_matches = 0
        w3_link_checked = 0
        oracle_w0_in_geo = 0
        oracle_w0_checked = 0
        poly_eq_geo = 0
        poly_eq_checked = 0
        for obj in POL_TEST + [0x148, 0x14F, 0x150, 0x1CB, 0x5C7]:
            rec = table.get(obj)
            if not rec:
                continue
            walk = words_consumed_by_mode(poly, rec["word_index"], mode)
            hi, lo = w3_halves(rec["w3"])
            span = span_to_next(table, obj)
            sc = score_case(walk["stats"], rec["w3"], walk["links"])
            oracle = oracle_by_id.get(f"{obj:#x}", {})
            geo_vals = [int(g["val"], 16) for g in oracle.get("geo", [])]
            fifo_vals = [int(g["val"], 16) for g in oracle.get("fifo", [])]
            w0 = rec["w0"]
            # MEASURED: helper stores table w0 to geo 0x800010 / 0x804000
            w0_hits = sum(1 for v in geo_vals if v == w0)
            # Are any poly-ROM body words written by i960 to geo/FIFO?
            body_words = []
            base = rec["word_index"] * 4
            for i in range(min(24, max(0, span or 24))):
                o = base + i * 4
                if o + 4 <= len(poly):
                    body_words.append(u32(poly, o))
            body_set = set(body_words)
            geo_set = set(geo_vals)
            fifo_set = set(fifo_vals)
            overlap_geo = body_set & geo_set
            overlap_fifo = body_set & fifo_set
            # exclude table/protocol/pad false positives
            pointer_forms = {
                rec["w2"],
                rec["w2"] & 0x7FFFFF,
                rec["word_index"],
                rec["w2"] | 0x00800000,
            }
            common_pad = {0, 0xFFFFFFFF, 0x00040401, 0x0000007F, 0x00F8013F}
            real_overlap_geo = (
                overlap_geo
                - pointer_forms
                - {rec["w0"], rec["w1"], rec["w3"]}
                - common_pad
            )
            real_overlap_fifo = (
                overlap_fifo
                - pointer_forms
                - {rec["w0"], rec["w1"], rec["w3"]}
                - common_pad
                # park continuation may write IEEE constants that also appear
                # in poly ROM (e.g. -1.0f 0xbf800000); still not a helper write
            )

            # Reject attrs that are IEEE floats in the vertex-magnitude range.
            # Measured false-positive mode: noskip/bit2 walks 0x3e4ccccd (0.2)
            # as attr and accidentally consumes pad → fake span match.
            bad_attr = False
            for a in walk["attrs_head"]:
                ai = int(a, 16) if isinstance(a, str) else int(a)
                if ai == 0:
                    continue
                try:
                    af = f32(ai)
                except Exception:
                    af = float("nan")
                if math.isfinite(af) and 1e-6 < abs(af) < 100.0 and (ai >> 24) < 0x40:
                    # looks like a small IEEE float, not an attr tag
                    if (ai & 0xff000000) < 0x40000000:
                        bad_attr = True
            span_match = None
            if span is not None and not bad_attr:
                span_checked += 1
                # MATCH if decode consumed words equals span to next id,
                # or span minus trailing pad (end marker + pad zeros/0x00040401)
                # Measured pol_test: 0x97d span=21, skip3 consumes 17 then pad.
                span_match = walk["words"] in (span, span - 1, span + 1) or (
                    walk["end_reason"] == "end_marker"
                    and 0 < walk["words"] <= span
                    and (span - walk["words"]) <= 8
                )
                if span_match:
                    span_matches += 1
            link_match = None
            if lo > 0:
                w3_link_checked += 1
                # MATCH if links == lo (prim count) or links == lo+/-1
                # or for known family: 0x97d span 21 → 1 quad link + end
                link_match = walk["links"] in (lo, max(1, lo - 1), lo + 1)
                if obj in (0x97D, 0x980, 0x981, 0x982, 0x985, 0x986) and lo == 2:
                    # tiny prim family: hex-calibrated 0x97d is ONE quad (+ pad end)
                    # w3 lo=2 is ambiguous; prefer links==1 as hex MATCH for 0x97d
                    if obj == 0x97D:
                        link_match = walk["links"] in (1, 2)
                if link_match:
                    w3_link_matches += 1
            if geo_vals:
                oracle_w0_checked += 1
                if w0_hits > 0:
                    oracle_w0_in_geo += 1
            if body_words:
                poly_eq_checked += 1
                if real_overlap_geo or real_overlap_fifo:
                    poly_eq_geo += 1
            rows[f"{obj:#x}"] = {
                "w3": hex(rec["w3"]),
                "w3_hi": hi,
                "w3_lo": lo,
                "word_index": hex(rec["word_index"]),
                "span_to_next": span,
                "decode_words": walk["words"],
                "decode_links": walk["links"],
                "decode_tris": walk["tris"],
                "end_reason": walk["end_reason"],
                "attrs_head": walk["attrs_head"],
                "span_match": span_match,
                "link_match": link_match,
                "score": sc,
                "oracle": {
                    "geo_count": oracle.get("counts", {}).get("geo"),
                    "fifo_count": oracle.get("counts", {}).get("fifo"),
                    "geo_vals_head": [g["val"] for g in oracle.get("geo", [])[:12]],
                    "fifo_vals_head": [g["val"] for g in oracle.get("fifo", [])[:12]],
                    "w0_in_geo": w0_hits > 0,
                    "poly_rom_words_in_geo": sorted(hex(x) for x in real_overlap_geo)[:8],
                    "poly_rom_words_in_fifo": sorted(hex(x) for x in real_overlap_fifo)[:8],
                    "poly_eq_oracle_writes": bool(real_overlap_geo or real_overlap_fifo),
                },
                "poly_head": dump_poly_stream(poly, rec["word_index"], 22),
            }
        results[mode] = {
            "objects": rows,
            "oracle_corroboration": {
                "span_match_rate": (span_matches / span_checked) if span_checked else None,
                "span_matches": span_matches,
                "span_checked": span_checked,
                "w3_link_match_rate": (w3_link_matches / w3_link_checked) if w3_link_checked else None,
                "w3_link_matches": w3_link_matches,
                "w3_link_checked": w3_link_checked,
                "w0_written_to_geo_rate": (
                    oracle_w0_in_geo / oracle_w0_checked if oracle_w0_checked else None
                ),
                "poly_body_words_written_rate": (
                    poly_eq_geo / poly_eq_checked if poly_eq_checked else None
                ),
                "interpretation": (
                    "Helper 0x7c60 MEASURED writes are table-record + protocol, "
                    "NOT polygon-ROM vertex floats. poly_body_words_written_rate "
                    "expected 0.0 — confirms decode cannot be pinned by i960 geo "
                    "vertex writes. Corroboration is span/w3 + tgp.c path only."
                ),
            },
        }
    return results


def build_hypothesis_table(results: dict, table: dict) -> list[dict]:
    return [
        {
            "bitfield": "polyROM[w2] & 0x7fffff",
            "meaning": "word_index into polygon ROM",
            "tag": "measured",
            "evidence": "object table 0x020e0004[id*16] w2; live oracle ldq; poly dumps at index*4",
        },
        {
            "bitfield": "polyROM[w2] & 0x00800000",
            "meaning": "polygon_rom vs polygon_ram bank select",
            "tag": "measured+tgp.c",
            "evidence": "tgp.c geometry source bit; all pol_test ids have bit set; MAME maps copro_data 0x800000",
        },
        {
            "bitfield": "table w0",
            "meaning": "object/geo word0; helper stores to geo 0x800010 and geo RAM 0x804000",
            "tag": "measured",
            "evidence": "live probe long-29: geo write value == table w0 for each id",
        },
        {
            "bitfield": "table w1",
            "meaning": "object/geo word1; helper stq word",
            "tag": "measured",
            "evidence": "live probe geo RAM 0x804004 == table w1",
        },
        {
            "bitfield": "table w2",
            "meaning": "poly address word; stored with ROM base 0x00800000|word_index",
            "tag": "measured",
            "evidence": "live probe geo RAM 0x804008 == table w2",
        },
        {
            "bitfield": "table w3",
            "meaning": "packed count pair. pol_test family: ((n-1)<<16)|n for n in {2,11,101,...}",
            "tag": "measured_family",
            "evidence": "live table dump + poly span deltas 21/111/1011",
        },
        {
            "bitfield": "table w3 (attract 0x148 etc)",
            "meaning": "NOT (n-1,n); semantic unknown (bounds/size/lod?)",
            "tag": "unproven",
            "evidence": "w3=0x014a0164 for 0x148; span formula from pol_test does not apply",
        },
        {
            "bitfield": "helper r11 after ldq",
            "meaning": "r11 starts as w3; shro16→r3, and 0xffff→r11 (lo); later subo 1,0,r11 → -1 for stq count word",
            "tag": "measured",
            "evidence": "disasm 0x7c60; recovered C polygon_object_submit.c; geo RAM 0x80400c==0xffffffff",
        },
        {
            "bitfield": "attr word (poly ROM body)",
            "meaning": "(attr&3)==0 end; attr&1 quad; (attr>>8)&3 link mode",
            "tag": "hypothesis_from_tgp.c",
            "evidence": "src/hardware/tgp.c geometry_execute_object; NOT re-observed as i960 writes",
        },
        {
            "bitfield": "attr high bits (poly ROM)",
            "meaning": "texture/color/flags? 0xe1001601 on 0x97d",
            "tag": "unproven",
            "evidence": "poly ROM hex; no MAME body; no oracle write of these words to geo",
        },
        {
            "bitfield": "3 words after attr (poly ROM)",
            "meaning": "skip3 when geometry_mode&3<2; contents look like normals (0,-1,0) on 0x97d",
            "tag": "hypothesis_from_tgp.c+hex",
            "evidence": "tgp.c skip path + poly_rom_hex.json 0x97d words 7..9",
        },
        {
            "bitfield": "FIFO 0x00800101",
            "meaning": "protocol word; class bits 0x01 align with tgp.c object-data case name",
            "tag": "measured_word+suggested_by_mame_class_name",
            "evidence": "pol_test task immediate; tgp.c case 0x01; MAME geo_object_data NAME only",
        },
        {
            "bitfield": "FIFO 0x01000202",
            "meaning": "protocol word; class bits 0x02 align with tgp.c direct-data case name",
            "tag": "measured_word+suggested_by_mame_class_name",
            "evidence": "pol_test task post-prim immediate; tgp.c case 0x02",
        },
        {
            "bitfield": "FIFO 0x1a003434",
            "meaning": "helper submit protocol; class 0x14; body unproven",
            "tag": "measured_word+unproven_semantics",
            "evidence": "disasm 0x7c7c; every submit",
        },
        {
            "bitfield": "FIFO 0x01800303 / 0x03000606",
            "meaning": "protocol family; class 0x03 / 0x06; bodies unproven",
            "tag": "measured_word+unproven_semantics",
            "evidence": "pol_test immediates; also attract/display paths",
        },
        {
            "bitfield": "FIFO floats 0x3e9eb852.. / 0x3f428f5c..",
            "meaning": "scale/translate-like immediates written by pol_test task; not poly-ROM verts",
            "tag": "measured_words+unproven_semantics",
            "evidence": "disasm 0x21a58-0x21a78 and 0x21bac-0x21bcc",
        },
        {
            "bitfield": "MAME geo_w bits 23-28 function, bit31 jump",
            "meaning": "host-side packing of geo control words",
            "tag": "suggested_by_mame",
            "evidence": "model2.cpp:822-896 geo_w; handlers bodies NOT in dump",
        },
        {
            "bitfield": "MAME geo_polygon_data / geo_object_data bodies",
            "meaning": "polygon command bitfields (texture/color/vertex list)",
            "tag": "absent_in_vendored_mame",
            "evidence": "model2.h names only; third_party dump has no handler bodies",
        },
        {
            "bitfield": "geometry_mode&3 < 2 → skip 3 words",
            "meaning": "float-link packet uses 3-word attr payload slot",
            "tag": "existing_boundary_model",
            "evidence": "src/hardware/tgp.c; NOT promoted by this P1 note",
        },
    ]


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    main_img = build(MAIN_PAIRS, 0x02400000)
    poly = build(POLY_PAIRS, 0x02000000)
    table = load_object_table(main_img)

    # Existing traces
    oracle_by_id = {}
    for p in sorted(OUT.glob("pol_test_g7c60_*.jsonl")):
        # filename pol_test_g7c60_97d.jsonl
        stem = p.stem  # pol_test_g7c60_97d
        id_part = stem.split("_")[-1]
        try:
            obj = int(id_part, 16)
        except ValueError:
            continue
        oracle_by_id[f"{obj:#x}"] = extract_oracle_sequences(p)

    # Also ingest any new iso_* traces if present
    for p in sorted(OUT.glob("pol_test_iso_*.jsonl")):
        stem = p.stem
        id_part = stem.split("_")[-1]
        try:
            obj = int(id_part, 16)
        except ValueError:
            continue
        key = f"{obj:#x}"
        rec = extract_oracle_sequences(p)
        if key not in oracle_by_id:
            oracle_by_id[key] = rec
        else:
            # merge: keep union of geo/fifo ordered? prefer iso as cleaner
            oracle_by_id[key]["iso"] = rec
            if rec["counts"]["geo"] or rec["counts"]["fifo"]:
                oracle_by_id[key] = {**rec, "merged_from": "iso"}

    modes = [
        "skip3_float_link",
        "noskip_float_link",
        "skip2_float_link",
        "skip4_float_link",
        "skip3_quad_bit2",
        "skip3_end_attr0",
        "skip3_fixed_link",
        "skip3_int16",
        "noskip_int16",
        "skip3_no_header",
        "skip3_quad_no_header",
        "noskip_quad_no_header",
        "skip3_tri_no_header",
    ]

    scores = score_against_oracle(table, poly, oracle_by_id, modes)

    # Rank by oracle corroboration: prefer span match, then w3 link match, then score
    ranked = []
    for mode, rec in scores.items():
        oc = rec["oracle_corroboration"]
        span_r = oc["span_match_rate"] or 0.0
        link_r = oc["w3_link_match_rate"] or 0.0
        pol_rows = [r for k, r in rec["objects"].items() if int(k, 16) in POL_TEST]
        mean_sc = (
            sum(r["score"]["total"] for r in pol_rows) / len(pol_rows) if pol_rows else 0.0
        )
        # tiny-prim special: 0x97d hex is 1 quad, span 21
        r97d = rec["objects"].get("0x97d", {})
        # Hex gold: skip3_float_link on 0x97d must yield links=1 (one quad)
        # from attr 0xe1001601 only — not float-as-attr walkers.
        hex_attr_ok = 0.0
        if r97d.get("attrs_head") == ["0xe1001601"] and r97d.get("decode_links") == 1:
            hex_attr_ok = 1.0
        tiny_ok = hex_attr_ok if (r97d.get("decode_links") in (1, 2)) else 0.0
        ranked.append(
            {
                "mode": mode,
                "span_match_rate": span_r,
                "w3_link_match_rate": link_r,
                "pol_mean_score": mean_sc,
                "tiny_97d_span_match": tiny_ok,
                "oracle_w0_in_geo": oc["w0_written_to_geo_rate"],
                "poly_body_in_oracle": oc["poly_body_words_written_rate"],
                "corroborated_score": 0.40 * span_r + 0.25 * link_r + 0.20 * mean_sc + 0.15 * tiny_ok,
            }
        )
    ranked.sort(key=lambda r: -r["corroborated_score"])

    # Hex-calibrated note for 0x97d
    r97 = scores["skip3_float_link"]["objects"].get("0x97d", {})
    poly_hex_97d = r97.get("poly_head", [])

    hyp = build_hypothesis_table(scores, table)

    # Compare oracle geo/FIFO values vs poly ROM body words (global)
    comparisons = {}
    for obj in POL_TEST:
        rec = table.get(obj)
        if not rec:
            continue
        oracle = oracle_by_id.get(f"{obj:#x}", {})
        geo_vals = {int(g["val"], 16) for g in oracle.get("geo", [])}
        fifo_vals = {int(g["val"], 16) for g in oracle.get("fifo", [])}
        body = []
        base = rec["word_index"] * 4
        span = span_to_next(table, obj) or 32
        for i in range(min(span, 48)):
            o = base + i * 4
            if o + 4 <= len(poly):
                body.append(u32(poly, o))
        body_set = set(body)
        common_pad = {
            0,
            0xFFFFFFFF,
            0x00040401,
            0x0000007F,
            0x00F8013F,
            0xBF800000,
            0x3F800000,
            0x44160000,
            0x45480000,
            0x3F99999A,
            0x01F001FF,
        }
        body_geo_real = (
            body_set & geo_vals
        ) - {rec["w0"], rec["w1"], rec["w2"], rec["w3"]} - common_pad
        body_fifo_real = (
            body_set & fifo_vals
        ) - {rec["w0"], rec["w1"], rec["w2"], rec["w3"]} - common_pad
        # exact equality of sequences for the 4 table words
        geo_seq = [g["val"] for g in oracle.get("geo", [])]
        comparisons[f"{obj:#x}"] = {
            "table": {k: hex(v) for k, v in rec.items() if k != "poly_rom"},
            "oracle_geo_seq": geo_seq[:20],
            "oracle_fifo_seq": [f["val"] for f in oracle.get("fifo", [])[:20]],
            "poly_rom_words": [hex(w) for w in body[:24]],
            "poly_rom_as_f32": [f32_of(w) for w in body[:24]],
            "geo_contains_w0": rec["w0"] in geo_vals,
            "geo_contains_w1": rec["w1"] in geo_vals,
            "geo_contains_w2": rec["w2"] in geo_vals,
            "geo_contains_poly_body_floats": bool(body_geo_real),
            "fifo_contains_poly_body_floats": bool(body_fifo_real),
            "poly_body_overlap_geo": [hex(x) for x in sorted(body_geo_real)],
            "poly_body_overlap_fifo": [hex(x) for x in sorted(body_fifo_real)],
            "verdict": (
                "geo/FIFO writes are table-record + protocol words, NOT "
                "polygon-ROM vertex/attr floats (pad/common constants excluded)"
                if not body_geo_real and not body_fifo_real
                else "UNEXPECTED residual poly-body words in oracle writes"
            ),
        }

    best = ranked[0]["mode"] if ranked else None
    # Fail-closed choice: i960 oracle does NOT write poly-ROM vertex words to
    # geo/FIFO (only table + protocol). Vertex-stream decode is therefore
    # UNPROVEN at a geometry boundary. Best host hypothesis remains the
    # hex-calibrated tgp.c path when 0x97d attr walk is exact.
    chosen = "skip3_float_link"
    if r97.get("attrs_head") != ["0xe1001601"] or r97.get("decode_links") != 1:
        chosen = best  # fall back only if hex calibration fails

    report = {
        "note": "P1 oracle-calibrated decode scoring. Fail-closed: host decode ≠ recovered C.",
        "park": str(PARK),
        "pol_test_task_disasm": POL_TEST_TASK,
        "object_table_pol_test": {
            f"{k:#x}": {
                kk: (hex(vv) if isinstance(vv, int) and kk != "poly_rom" else vv)
                for kk, vv in v.items()
            }
            for k, v in table.items()
            if k in POL_TEST or k in (0x148, 0x14F, 0x150, 0x1CB, 0x5C7)
        },
        "known_fifo_words": KNOWN_FIFO,
        "oracle_sequences": oracle_by_id,
        "poly_vs_oracle": comparisons,
        "hypothesis_table": hyp,
        "decode_scores": scores,
        "ranking": ranked,
        "best_mode_auto": best,
        "chosen_mode": chosen,
        "hex_calibration_97d": {
            "poly_words_f32": poly_hex_97d,
            "skip3_links": r97.get("decode_links"),
            "skip3_words": r97.get("decode_words"),
            "skip3_span_match": r97.get("span_match"),
            "skip3_tris": r97.get("decode_tris"),
        },
        "fail_closed": (
            "No decode is oracle-pinned at a geometry vertex-write boundary. "
            "Measured i960 helper/task writes are table-record + FIFO protocol "
            "only — NOT poly-ROM vertex/attr floats. Auto span matches that "
            "treat IEEE floats as attr words are FALSE POSITIVES. "
            "skip3_float_link remains the best host hypothesis "
            "(hex-calibrated 0x97d ±0.2 XZ quad + tgp.c path); NOT recovered C."
        ),
        "vertex_stream_corroboration": (
            "ABSENT/UNPROVEN — oracle geo/FIFO sequences do not contain "
            "polygon-ROM body floats for pol_test ids."
        ),
    }
    (OUT / "oracle_decode_score.json").write_text(json.dumps(report, indent=2))
    (OUT / "hypothesis_table.json").write_text(json.dumps(hyp, indent=2))

    print("=== ranking (oracle-corroborated) ===")
    for r in ranked:
        print(
            f"  {r['mode']:22s} corr={r['corroborated_score']:.4f} "
            f"span={r['span_match_rate']:.3f} link={r['w3_link_match_rate']:.3f} "
            f"tiny97d={r['tiny_97d_span_match']:.0f} pol={r['pol_mean_score']:.3f}"
        )
    print(f"best_mode_auto={best}")
    print(f"chosen_mode={chosen}")
    print("=== poly vs oracle verdicts ===")
    for k, c in comparisons.items():
        print(
            f"  {k}: w0_in_geo={c['geo_contains_w0']} "
            f"body_in_geo={c['geo_contains_poly_body_floats']} "
            f"body_in_fifo={c['fifo_contains_poly_body_floats']} -> {c['verdict'][:60]}"
        )
    print(f"wrote {OUT / 'oracle_decode_score.json'}")
    print(f"wrote {OUT / 'hypothesis_table.json'}")


if __name__ == "__main__":
    main()
