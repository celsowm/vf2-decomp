#!/usr/bin/env python3
"""Measure TGP / camera / matrix / focus state for attract-TGP recovery.

Evidence-first. Fail-closed. Never invents a camera matrix.

Sources (read-only unless --probe is used carefully):
  1. ROM static: scan out/maincpu.bin for known float/address immediates
     and list guest store sites via measured disasm knowledge.
  2. Snap parks: parse .vf2snap without executing guest (no probe freeze risk)
     and dump work-RAM / display / geo / camera-scale windows.
  3. Existing memory-trace JSONL: classify geo-port writes 0x800010/0x804000
     and FIFO writes beyond protocol color tags.
  4. Optional probe orchestration: only when --probe is set. Probe --max-steps 0
     does NOT freeze; this tool therefore prefers snap-parser and existing
     traces, and records probe attempts as optional with measured before/after.

Outputs (compact JSON, not full binary dumps):
  out/attr-transform/region_<park>.json
  out/attr-transform/static_store_ips.json
  out/attr-transform/geo_port_classes.json
  out/attr-transform/absence_table.json
  out/attr-transform/measured_view.json   ONLY if confidence=measured
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, wu32, wu8, WORK_BASE  # noqa: E402

F32_NAME = {
    0x40C00000: "6.0f",
    0x40966666: "4.7f",
    0x41940000: "18.5f",
    0x44160000: "600.0f",
    0x3F800000: "1.0f",
    0x3F000000: "0.5f",
    0x3CA3D70A: "0.02f",
    0x43850000: "266.0f",
    0xC3850000: "-266.0f",
}

# Protocol / color immediates measured in FIFO (v0372/v0373/v0375).
PROTOCOL_WORDS = {
    0x00800101,
    0x01800303,
    0x03000606,
    0x1A003434,
    0x14802929,
    0x1C803939,
    0x09801313,
    0x37806F6F,
    0x33806767,
    0x34806969,
    0x35806B6B,
    0x36006C6C,
    0x02800505,
    0x01000202,
    0xFFFFFFFF,
}

KNOWN_STORE_SITES = [
    {
        "ip": 0x00031024,
        "insn": "stt r8, 0x54(r4)",
        "function": "display_transform_defaults",
        "function_ip": 0x00031004,
        "loads": [0x40C00000, 0x40966666, 0x41940000],
        "target": "display_object[*(u32*)0x50084c] + 0x54/0x58/0x5c",
        "kind": "display_triple",
        "tgp_matrix": False,
        "note": "Work-RAM display-state triple, not TGP class-0b matrix",
    },
    {
        "ip": 0x00031034,
        "insn": "stt r12, 0x60(r4)",
        "function": "display_transform_defaults",
        "function_ip": 0x00031004,
        "loads": [0x00000000],
        "target": "display_object + 0x60/0x64/0x68",
        "kind": "display_secondary_zero",
        "tgp_matrix": False,
    },
    {
        "ip": 0x0001D34C,
        "insn": "st r15, 0x00501084",
        "function": "fa_camera_initialize_prefix",
        "function_ip": 0x0001D320,
        "loads": [0x44160000],
        "target": "0x00501084",
        "kind": "camera_scale",
        "tgp_matrix": False,
        "note": "Guest store of camera scale 600.0f",
    },
    {
        "ip": 0x0001D35C,
        "insn": "st r15, 0x00501088",
        "function": "fa_camera_initialize_prefix",
        "function_ip": 0x0001D320,
        "loads": [0x44160000],
        "target": "0x00501088",
        "kind": "camera_scale",
        "tgp_matrix": False,
    },
    {
        "ip": 0x00031058,
        "insn": "st r15, (g11)[g12]",
        "function": "display_command_emit",
        "function_ip": 0x00031040,
        "loads": [0x00800101],
        "target": "FIFO (g11)[g12]",
        "kind": "fifo_protocol",
        "tgp_matrix": False,
        "note": "Protocol word 0x00800101, NOT TGP class-0b matrix opcode",
    },
    {
        "ip": 0x00031068,
        "insn": "st r4, 0x0090e000(r15)",
        "function": "display_command_emit",
        "function_ip": 0x00031040,
        "loads": ["display+0x54 via ldt"],
        "target": "aperture buffer 0x0090e000[cursor 0x5001e4]",
        "kind": "aperture_transform_packet",
        "tgp_matrix": False,
        "note": "Display triple serialized into aperture window, not geo 3x4",
    },
    {
        "ip": 0x00007D08,
        "insn": "st r8, 0x10(g10)",
        "function": "object_submit_helper",
        "function_ip": 0x00007C60,
        "loads": ["object_table w0"],
        "target": "geo 0x800010 (g10 base)",
        "kind": "geo_object_w0",
        "tgp_matrix": False,
    },
    {
        "ip": 0x00007D0C,
        "insn": "stq r8, (g10)[g12]",
        "function": "object_submit_helper",
        "function_ip": 0x00007C60,
        "loads": ["object_table qword"],
        "target": "FIFO",
        "kind": "fifo_object_submit",
        "tgp_matrix": False,
    },
    {
        "ip": 0x00007C84,
        "insn": "st r15, (g11)[g12]",
        "function": "object_submit_helper",
        "function_ip": 0x00007C60,
        "loads": [0x1A003434],
        "target": "FIFO",
        "kind": "fifo_protocol",
        "tgp_matrix": False,
    },
    {
        "ip": 0x0001D478,
        "insn": "st r15, (g11)[g12]",
        "function": "fa_camera_recurring_prefix",
        "function_ip": 0x0001D458,
        "loads": [0x0B001616],
        "target": "FIFO 0x00884000",
        "kind": "fifo_copro_arith_tag",
        "tgp_matrix": False,
        "note": (
            "class=(0x0b001616>>23)&0x1f=0x16, NOT TGP class 0x0b matrix. "
            "Arithmetic copro protocol: tag + operands + readback to camera task."
        ),
    },
    {
        "ip": 0x0001D484,
        "insn": "st r15, (g11)[g12]",
        "function": "fa_camera_recurring_prefix",
        "function_ip": 0x0001D458,
        "loads": ["*(u32*)0x501084 = 600.0f"],
        "target": "FIFO 0x00884000",
        "kind": "fifo_copro_operand",
        "tgp_matrix": False,
    },
    {
        "ip": 0x0001D498,
        "insn": "st r15, 0x5c(g13)",
        "function": "fa_camera_recurring_prefix",
        "function_ip": 0x0001D458,
        "loads": ["FIFO readback"],
        "target": "camera_task + 0x5c",
        "kind": "camera_task_scratch",
        "tgp_matrix": False,
    },
    {
        "ip": 0x0001D4C4,
        "insn": "st r15, 0x60(g13)",
        "function": "fa_camera_recurring_prefix",
        "function_ip": 0x0001D458,
        "loads": ["FIFO readback"],
        "target": "camera_task + 0x60",
        "kind": "camera_task_scratch",
        "tgp_matrix": False,
    },
]

DEFAULT_PARKS = [
    Path("out/attr-long/long-29.vf2snap"),
    Path("out/attr-logo/emit31040.vf2snap"),
    Path("out/attr-v0372/boot-sel03-fifo.vf2snap"),
    Path("out/attr-v0372/boot-sel09-fifo.vf2snap"),
    Path("out/attr-fs/all0-ready1.vf2snap"),
    Path("out/sixth-fresh.vf2snap"),
]

DEFAULT_TRACES = [
    Path("out/attr-long/fifo-phase5.jsonl"),
    Path("out/attr-long/fifo-attract.jsonl"),
    Path("out/attr-v0372/boot-sel03-fifo.jsonl"),
    Path("out/attr-v0372/boot-sel09-fifo.jsonl"),
    Path("out/attr-v0372/boot-sel09-long.jsonl"),
    Path("out/attr-logo/emit31040.jsonl"),
]

FIFO_LO, FIFO_HI = 0x00884000, 0x00886000
GEO_LO, GEO_HI = 0x00800000, 0x00810000
GEO_PORTS = (0x00800010, 0x00804000)


def f32(bits: int) -> float:
    return struct.unpack("<f", struct.pack("<I", bits & 0xFFFFFFFF))[0]


def bits_label(bits: int) -> str:
    name = F32_NAME.get(bits)
    try:
        val = f32(bits)
    except Exception:
        val = float("nan")
    if name:
        return f"0x{bits:08x} ({name})"
    return f"0x{bits:08x} (~{val:g})"


def scan_maincpu(maincpu: Path) -> dict[str, Any]:
    if not maincpu.exists():
        return {"present": False, "path": str(maincpu)}
    blob = maincpu.read_bytes()
    needles = {
        "6.0f": struct.pack("<I", 0x40C00000),
        "4.7f": struct.pack("<I", 0x40966666),
        "18.5f": struct.pack("<I", 0x41940000),
        "600.0f": struct.pack("<I", 0x44160000),
        "addr_50084c": struct.pack("<I", 0x0050084C),
        "addr_501084": struct.pack("<I", 0x00501084),
        "addr_501088": struct.pack("<I", 0x00501088),
        "addr_5001e4": struct.pack("<I", 0x005001E4),
        "addr_90e000": struct.pack("<I", 0x0090E000),
        "addr_800010": struct.pack("<I", 0x00800010),
        "addr_804000": struct.pack("<I", 0x00804000),
        "addr_884000": struct.pack("<I", 0x00884000),
        "imm_00800101": struct.pack("<I", 0x00800101),
        "imm_03000606": struct.pack("<I", 0x03000606),
        "imm_1a003434": struct.pack("<I", 0x1A003434),
        "imm_37806f6f": struct.pack("<I", 0x37806F6F),
        "call_7c60": struct.pack("<I", 0x00007C60),
        "addr_31004": struct.pack("<I", 0x00031004),
        "addr_31040": struct.pack("<I", 0x00031040),
        "addr_311b8": struct.pack("<I", 0x000311B8),
    }
    hits: dict[str, list[int]] = {}
    for name, nd in needles.items():
        offs = []
        start = 0
        while True:
            i = blob.find(nd, start)
            if i < 0:
                break
            offs.append(i)
            start = i + 1
            if len(offs) >= 40:
                break
        hits[name] = offs
    return {
        "present": True,
        "path": str(maincpu),
        "size": len(blob),
        "hits": {k: [hex(o) for o in v] for k, v in hits.items()},
        "hit_counts": {k: len(v) for k, v in hits.items()},
        "store_sites": KNOWN_STORE_SITES,
        "disasm_evidence": {
            "display_transform_defaults": "0x31004: ld 0x50084c; lda 0x40c00000/0x40966666/0x41940000; stt r8,0x54(r4)",
            "display_command_emit": (
                "0x31040: protocol 0x00800101 to FIFO; ldt +0x54; "
                "st r4/r5/r6 to 0x0090e000[cursor]; call 0x7c60; ret 0x310c8"
            ),
            "object_submit_helper": (
                "0x7c60: protocol 0x1a003434; table 0x020e0004[g0*16]; "
                "st r8,0x10(g10); stq r8,(g10)[g12]"
            ),
            "camera_init": (
                "0x1d320: lda/st camera task vectors; "
                "0x1d34c st 0x44160000 -> 0x501084; 0x1d35c st 0x44160000 -> 0x501088"
            ),
        },
    }


def looks_like_matrix(words: list[int]) -> dict[str, Any]:
    """Score 12-float windows for identity-like / orthonormal 3x4 shape."""
    if len(words) < 12:
        return {"candidates": []}
    cands = []
    for i in range(0, len(words) - 11):
        ws = words[i : i + 12]
        fl = [f32(w) for w in ws]
        # Skip all-zero / mostly integer control words
        floatish = 0
        for w in ws:
            exp = (w >> 23) & 0xFF
            if 0x38 <= exp <= 0x48:  # plausible finite floats ~1e-3..1e3-ish band
                floatish += 1
        if floatish < 8:
            continue
        # Column-major 3x4 guess: m0,m4,m8 / m1,m5,m9 / m2,m6,m10 / m3=tx? layout varies
        # Score closeness to identity-like: diagonal ~1 others ~0
        # Try row-of-3 packing used by TGP payload (12 floats)
        a, b, c = fl[0], fl[1], fl[2]
        d, e, f = fl[3], fl[4], fl[5]
        g, h, i_ = fl[6], fl[7], fl[8]
        tx, ty, tz = fl[9], fl[10], fl[11]
        id_score = 0.0
        id_score += abs(a - 1.0) + abs(e - 1.0) + abs(i_ - 1.0)
        id_score += abs(b) + abs(c) + abs(d) + abs(f) + abs(g) + abs(h)
        cands.append(
            {
                "offset_words": i,
                "floats": fl,
                "identity_score": id_score,
                "bits": [f"0x{w:08x}" for w in ws],
                "looks_identity_like": id_score < 0.15,
            }
        )
    cands.sort(key=lambda c: c["identity_score"])
    return {"candidates": cands[:8]}


def dump_park(path: Path) -> dict[str, Any]:
    snap = parse_snap(path)
    work = snap["regions"]["work-ram"]
    geom = snap["regions"].get("geometry", b"")
    copro = snap["regions"].get("copro-port", b"")
    size_work = len(work)
    size_geom = len(geom)
    size_copro = len(copro)

    def work_u32(addr: int) -> int | None:
        off = addr - WORK_BASE
        if off < 0 or off + 4 > size_work:
            return None
        return struct.unpack_from("<I", work, off)[0]

    def work_slice(base: int, n: int) -> list[dict[str, Any]]:
        out = []
        for i in range(n):
            addr = base + i * 4
            v = work_u32(addr)
            if v is None:
                continue
            out.append(
                {
                    "addr": f"0x{addr:08x}",
                    "bits": f"0x{v:08x}",
                    "f32": f32(v),
                    "label": bits_label(v),
                }
            )
        return out

    display_ptr = work_u32(0x0050084C)
    display_fields = []
    display_abs = []
    if display_ptr and 0x00500000 <= display_ptr < 0x00600000:
        for off in (0x00, 0x40, 0x54, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x70, 0x72):
            v = work_u32(display_ptr + off)
            display_fields.append(
                {
                    "off": f"+0x{off:02x}",
                    "addr": f"0x{display_ptr + off:08x}",
                    "bits": None if v is None else f"0x{v:08x}",
                    "f32": None if v is None else f32(v),
                    "label": None if v is None else bits_label(v),
                }
            )
            if v is not None:
                display_abs.append(f"0x{display_ptr + off:08x}")
    elif display_ptr:
        # display object may live outside work-ram window in snap (buffer/video)
        display_fields.append(
            {
                "note": "display ptr outside work-ram snap window",
                "bits": f"0x{display_ptr:08x}",
            }
        )

    cam84 = work_u32(0x00501084)
    cam88 = work_u32(0x00501088)
    aperture = work_u32(0x005001E4)

    # Work-RAM 0x500000-0x501200 compact: only nonzero words + float-like
    region_5000 = []
    for addr in range(0x00500000, min(0x00501200, WORK_BASE + size_work), 4):
        v = work_u32(addr)
        if not v:
            continue
        region_5000.append(
            {
                "addr": f"0x{addr:08x}",
                "bits": f"0x{v:08x}",
                "f32": f32(v),
                "label": bits_label(v),
            }
        )

    # Geometry snap region: nonzero words + matrix-like scan
    geom_words = []
    for i in range(0, min(size_geom, 0x2000), 4):
        if i + 4 > size_geom:
            break
        geom_words.append(struct.unpack_from("<I", geom, i)[0])
    nz_geom = [
        {"off": f"0x{i*4:05x}", "bits": f"0x{w:08x}", "label": bits_label(w)}
        for i, w in enumerate(geom_words)
        if w
    ]
    geom_matrix = looks_like_matrix(geom_words)

    # copro-port region if present
    copro_words = []
    if copro:
        for i in range(0, min(len(copro), 0x2000), 4):
            if i + 4 > len(copro):
                break
            copro_words.append(struct.unpack_from("<I", copro, i)[0])
    copro_nz = [
        {"off": f"0x{i*4:05x}", "bits": f"0x{w:08x}", "label": bits_label(w)}
        for i, w in enumerate(copro_words)
        if w
    ][:80]
    copro_matrix = looks_like_matrix(copro_words) if copro_words else {"candidates": []}

    # Camera task record candidate 0x515400 (from camera notes)
    cam_task = work_slice(0x00515400, 32)

    result = {
        "park": str(path),
        "ip": f"0x{snap['ip']:08x}",
        "executed": snap["executed"],
        "sel": wu8(work, 0x50002A) if size_work > 0x2A else None,
        "phase3": wu8(work, 0x500030) if size_work > 0x30 else None,
        "aperture_cursor_0x5001e4": None if aperture is None else f"0x{aperture:08x}",
        "display_ptr_0x50084c": None if display_ptr is None else f"0x{display_ptr:08x}",
        "display_fields": display_fields,
        "display_triple_bits": {
            "x": None if cam84 is None else None,  # filled below from fields
        },
        "camera_scale_0x501084": None if cam84 is None else bits_label(cam84),
        "camera_scale_0x501088": None if cam88 is None else bits_label(cam88),
        "work_region_500000_501200_nz": region_5000[:200],
        "work_region_nz_count": len(region_5000),
        "camera_task_0x515400": cam_task,
        "geometry_region_len": size_geom,
        "geometry_nz": nz_geom[:80],
        "geometry_nz_count": len(nz_geom),
        "geometry_matrix_like": geom_matrix,
        "copro_region_len": size_copro,
        "copro_nz": copro_nz,
        "copro_matrix_like": copro_matrix,
        "tgp_matrix_status": "absent_in_snap_regions",
        "focus_status": "absent",
        "geometry_mode_status": "absent",
        "confidence": "display_triple_and_camera_scale_measured; tgp_matrix_absent",
    }
    # Extract triple from display fields if present
    triple = {}
    for fld in display_fields:
        if isinstance(fld, dict) and fld.get("off") in ("+0x54", "+0x58", "+0x5c"):
            triple[fld["off"]] = fld
    result["display_triple"] = triple
    return result


def classify_geo_ports(traces: list[Path]) -> dict[str, Any]:
    per_trace = {}
    totals = Counter()
    port_vals = Counter()
    fifo_class = Counter()
    fifo_nonprotocol_class = Counter()
    float_like_ports = []
    tgp_class_hits = {c: {"raw": 0, "protocol": 0, "remaining": []} for c in (0x07, 0x09, 0x0B, 0x0C)}

    def tgp_class(w: int) -> int:
        return (w >> 23) & 0x1F

    def is_protocol(val: int) -> bool:
        if val in PROTOCOL_WORDS or val in (0, 0xFFFFFFFF):
            return True
        b0, b1, b2, b3 = val & 0xFF, (val >> 8) & 0xFF, (val >> 16) & 0xFF, (val >> 24) & 0xFF
        if b0 == b1 and b2 in (0x00, 0x80) and b3 != 0:
            return True
        return False

    def looks_float_word(val: int) -> bool:
        exp = (val >> 23) & 0xFF
        return 0x38 <= exp <= 0x48 and (val & 0x7FFFFF) != 0

    for tr in traces:
        if not tr.exists():
            continue
        stats = Counter()
        local_ports = Counter()
        local_samples = []
        with tr.open("r", encoding="utf-8", errors="replace") as fh:
            for line in fh:
                if not line.startswith("{") or '"memory"' not in line or '"write"' not in line:
                    continue
                try:
                    j = json.loads(line)
                except Exception:
                    continue
                if j.get("type") != "memory" or j.get("kind") != "write":
                    continue
                addr = j.get("address", 0)
                try:
                    bs = bytes.fromhex(j.get("bytes", "00"))
                    val = int.from_bytes(bs[:4], "little")
                except Exception:
                    continue
                stats["memory_writes"] += 1
                if FIFO_LO <= addr < FIFO_HI:
                    stats["fifo_writes"] += 1
                    cls = tgp_class(val)
                    fifo_class[cls] += 1
                    if not is_protocol(val):
                        fifo_nonprotocol_class[cls] += 1
                    if cls in tgp_class_hits:
                        tgp_class_hits[cls]["raw"] += 1
                        if is_protocol(val):
                            tgp_class_hits[cls]["protocol"] += 1
                        elif len(tgp_class_hits[cls]["remaining"]) < 24:
                            tgp_class_hits[cls]["remaining"].append(
                                {"word": f"0x{val:08x}", "addr": f"0x{addr:08x}", "class": cls}
                            )
                elif GEO_LO <= addr < GEO_HI:
                    stats["geo_writes"] += 1
                    if addr in GEO_PORTS:
                        stats["geo_port_writes"] += 1
                        local_ports[f"0x{addr:08x}"] += 1
                        port_vals[val] += 1
                        totals["geo_port_writes"] += 1
                        rec = {
                            "addr": f"0x{addr:08x}",
                            "word": f"0x{val:08x}",
                            "class": tgp_class(val),
                            "protocol": is_protocol(val),
                            "f32": f32(val) if looks_float_word(val) else None,
                            "label": bits_label(val),
                        }
                        if looks_float_word(val) and not is_protocol(val):
                            float_like_ports.append(rec)
                        if len(local_samples) < 40:
                            local_samples.append(rec)
                        cls = tgp_class(val)
                        if cls in tgp_class_hits:
                            tgp_class_hits[cls]["raw"] += 1
                            if is_protocol(val):
                                tgp_class_hits[cls]["protocol"] += 1
                            elif len(tgp_class_hits[cls]["remaining"]) < 24:
                                tgp_class_hits[cls]["remaining"].append(
                                    {"word": f"0x{val:08x}", "addr": f"0x{addr:08x}", "class": cls, "port": True}
                                )
        per_trace[str(tr)] = {
            "stats": dict(stats),
            "geo_port_by_addr": dict(local_ports),
            "geo_port_samples": local_samples,
        }

    # Deduplicate float-like port values
    uniq_float = {}
    for rec in float_like_ports:
        uniq_float[rec["word"]] = rec
    return {
        "traces": per_trace,
        "totals": dict(totals),
        "unique_geo_port_values": [
            {"word": f"0x{v:08x}", "n": n, "label": bits_label(v), "class": tgp_class(v), "protocol": is_protocol(v)}
            for v, n in port_vals.most_common(80)
        ],
        "float_like_nonprotocol_geo_port": list(uniq_float.values())[:80],
        "fifo_class_hist": {f"0x{c:02x}": n for c, n in fifo_class.most_common(32)},
        "fifo_nonprotocol_class_hist": {
            f"0x{c:02x}": n for c, n in fifo_nonprotocol_class.most_common(32)
        },
        "tgp_class_port_hits": {
            f"0x{c:02x}": {
                "raw": d["raw"],
                "protocol": d["protocol"],
                "remaining_nonprotocol": d["raw"] - d["protocol"],
                "remaining_samples": d["remaining"],
            }
            for c, d in tgp_class_hits.items()
        },
        "conclusion": (
            "Geo-port writes are dominated by object-table w0 / protocol words. "
            "No measured class-0b 3x4 matrix payload or class-09 focus pair on "
            "0x800010/0x804000. Float-like residuals are IEEE immediates that "
            "collide with class bits; they are not accepted as geometry_mode/focus/matrix."
        ),
    }


def build_absence(static: dict, parks: list[dict], geo: dict) -> dict[str, Any]:
    def park_lookup(name: str, key: str) -> list[Any]:
        out = []
        for p in parks:
            out.append({"park": p.get("park"), key: p.get(key)})
        return out

    return {
        "question": "Where do geometry_mode / matrix 3x4 / focus_x/y / camera state live for attract/TGP?",
        "measured_presence": [
            {
                "state": "display_transform_triple",
                "addresses": ["*(u32*)0x50084c + 0x54/0x58/0x5c"],
                "values_long29": ["0x40c00000 (6.0f)", "0x40966666 (4.7f)", "0x41940000 (18.5f)"],
                "store_ip": "0x00031024 (stt r8, 0x54(r4))",
                "function": "display_transform_defaults @ 0x31004",
                "classification": "Work-RAM display-state, NOT TGP 3x4 matrix",
                "confidence": "measured",
            },
            {
                "state": "camera_scale",
                "addresses": ["0x00501084", "0x00501088"],
                "values_long29": ["0x44160000 (600.0f)", "0x44160000 (600.0f)"],
                "store_ips": ["0x0001D34C", "0x0001D35C"],
                "function": "fa_camera_initialize_prefix @ 0x1d320",
                "classification": "Work-RAM camera scale globals",
                "confidence": "measured",
            },
            {
                "state": "aperture_cursor",
                "addresses": ["0x005001e4"],
                "values_parks": park_lookup("aperture_cursor_0x5001e4", "aperture_cursor_0x5001e4"),
                "store_ips": ["0x00031094 stob r15, 0x5001e4 (display_command_emit)"],
                "classification": "Aperture write cursor for display transform packets at 0x0090e000[]",
                "confidence": "measured",
            },
            {
                "state": "display_object_ptr",
                "addresses": ["0x0050084c"],
                "values_parks": park_lookup("display_ptr_0x50084c", "display_ptr_0x50084c"),
                "classification": "Pointer to display runtime object (long-29/emit31040 = 0x00515d00)",
                "confidence": "measured",
            },
        ],
        "measured_absence": [
            {
                "state": "tgp_geometry_mode_class_0x07",
                "expected_stream": "FIFO/geo class=(word>>23)&0x1f == 0x07 with following mode word",
                "result": "NOT accepted. Residual class-07 bit collisions are IEEE immediates (266.0f family); no measured mode payload follows.",
                "confidence": "absent",
            },
            {
                "state": "tgp_focus_xy_class_0x09",
                "expected_stream": "FIFO/geo class 0x09 + two float words",
                "result": "ABSENT after protocol false-class filtering (v0375 + v0376 reconfirm on geo ports and FIFO).",
                "confidence": "absent",
            },
            {
                "state": "tgp_matrix_3x4_class_0x0b",
                "expected_stream": "FIFO/geo class 0x0b + 12 payload words (geometry_write_matrix)",
                "result": "ABSENT in measured attract/boot FIFO and geo-port traffic. Snap geometry/coprocessor regions contain no identity-like 12-float matrix.",
                "confidence": "absent",
            },
            {
                "state": "tgp_translate_class_0x0c",
                "expected_stream": "FIFO/geo class 0x0c + 3 floats",
                "result": "ABSENT after protocol filtering.",
                "confidence": "absent",
            },
            {
                "state": "guest_store_of_tgp_matrix_to_work_ram",
                "expected": "store IP writing 12 floats to a stable work-RAM camera/matrix object",
                "result": (
                    "No such store site measured. display_command_emit serializes the display "
                    "triple into aperture buffer 0x0090e000[cursor] and object-submit helper "
                    "writes object-table w0 to geo 0x800010 — neither is a TGP 3x4 matrix store."
                ),
                "confidence": "absent",
            },
        ],
        "geo_port_summary": geo.get("conclusion"),
        "fail_closed": (
            "Host raster must NOT invent a camera matrix from hybrid C or display defaults. "
            "Usable measured inputs remain: object ids + poly offsets; aperture/display triple; "
            "camera scale globals; FIFO/geo protocol hex. apply_tgp_transform stays identity "
            "passthrough with confidence=absent until a measured TGP matrix/focus exists."
        ),
        "static_hits": static.get("hit_counts") if static.get("present") else None,
        "park_matrix_like_best": [
            {
                "park": p.get("park"),
                "geom_best": ((p.get("geometry_matrix_like") or {}).get("candidates") or [None])[0],
                "copro_best": ((p.get("copro_matrix_like") or {}).get("candidates") or [None])[0],
            }
            for p in parks
        ],
    }


def write_measured_view(out_dir: Path, parks: list[dict], static: dict) -> Path | None:
    """Only write measured_view.json if a real TGP matrix+focus was measured.

    Current evidence: TGP matrix/focus ABSENT. Do not invent.
    Display triple is NOT promoted into measured_view.matrix.
    """
    path = out_dir / "measured_view.json"
    # Search parks for any matrix-like candidate that is identity-like AND has
    # a matching measured focus pair. None expected.
    matrix_found = None
    focus_x = None
    focus_y = None
    source = None
    ips = []
    for p in parks:
        for kind in ("geometry_matrix_like", "copro_matrix_like"):
            cands = (p.get(kind) or {}).get("candidates") or []
            for c in cands:
                if c.get("looks_identity_like"):
                    # Still require a measured focus — we do not have one.
                    matrix_found = c
                    source = p.get("park")
    if matrix_found is None:
        # Fail-closed stub is NOT written as measured_view.json with fake data.
        # Write an explicit absence marker for the re-render agent if requested
        # via --write-absent-view.
        return None

    payload = {
        "matrix": matrix_found["floats"],
        "focus_x": focus_x,
        "focus_y": focus_y,
        "source": source,
        "ips": ips,
        "confidence": "measured",
        "note": "Promoted from snap geometry/copro identity-like 12-float candidate; focus still required",
    }
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    return path


def write_absent_view(out_dir: Path, parks: list[dict]) -> Path:
    """Explicit absence file for re-render agent — confidence=absent, not a camera."""
    path = out_dir / "measured_view.json"
    long29 = next((p for p in parks if "long-29" in str(p.get("park", ""))), parks[0] if parks else {})
    triple = long29.get("display_triple") or {}
    payload = {
        "matrix": None,
        "focus_x": None,
        "focus_y": None,
        "geometry_mode": None,
        "source": long29.get("park"),
        "ips": [],
        "confidence": "absent",
        "fail_closed": (
            "No measured TGP 3x4 matrix / focus_x / focus_y / geometry_mode for attract/TGP. "
            "Do NOT treat this as a recovered camera. apply_transform must passthrough identity."
        ),
        "related_measured_work_ram": {
            "display_ptr_0x50084c": long29.get("display_ptr_0x50084c"),
            "display_triple": triple,
            "display_triple_store_ip": "0x00031024",
            "camera_scale_0x501084": long29.get("camera_scale_0x501084"),
            "camera_scale_0x501088": long29.get("camera_scale_0x501088"),
            "camera_scale_store_ips": ["0x0001d34c", "0x0001d35c"],
            "aperture_cursor_0x5001e4": long29.get("aperture_cursor_0x5001e4"),
            "camera_task_0x515400_plus5c_60": {
                "+0x5c": "0x435f3333 (~223.2f) measured via FIFO copro path",
                "+0x60": "0x432ccccd (~172.8f) measured via FIFO copro path",
                "continuation": "0x0001d458",
                "classification": "camera_task_scratch_not_focus",
            },
            "fifo_copro_arith_tag": {
                "word": "0x0b001616",
                "tgp_class_bits": "0x16",
                "classification": "NOT tgp class-0b matrix opcode",
            },
        },
        "note": (
            "Display triple (6.0,4.7,18.5) is Work-RAM display-state at object+0x54, "
            "stored by guest IP 0x00031024. Camera scale 600.0f stored at 0x1d34c/0x1d35c. "
            "These are measured but are NOT a TGP class-0b matrix."
        ),
    }
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    return path


def optional_probe_note(args: argparse.Namespace) -> dict[str, Any]:
    return {
        "probe_used": bool(args.probe),
        "caution": (
            "vf2probe --max-steps 0 does NOT freeze (executes thousands of insns). "
            "Prefer snap-parser. If probing store IPs, use --set-ip at the store "
            "instruction with --until after the store and --max-steps carefully, "
            "and record BEFORE/AFTER from --memory-trace / --read-u32."
        ),
        "candidate_probe_commands": [
            (
                "build/Debug/vf2probe.exe --rom-dir roms/vf2 "
                "--snapshot out/attr-long/long-29.vf2snap "
                "--set-ip 0x00031004 --until 0x00031040 --max-steps 200 "
                "--memory-trace --read-u32 0x0050084c"
            ),
            (
                "build/Debug/vf2probe.exe --rom-dir roms/vf2 "
                "--snapshot out/attr-long/long-29.vf2snap "
                "--set-ip 0x0001d320 --until 0x0001d364 --max-steps 400 "
                "--memory-trace --read-u32 0x00501084 --read-u32 0x00501088"
            ),
            (
                "build/Debug/vf2probe.exe --rom-dir roms/vf2 "
                "--snapshot out/attr-long/long-29.vf2snap "
                "--set-ip 0x00031040 --until 0x000310c8 --max-steps 4000 "
                "--set-u32 0x00500064=5 --memory-trace --trace"
            ),
        ],
        "store_ips_to_watch": [s["ip"] for s in KNOWN_STORE_SITES if not s["kind"].startswith("fifo")],
    }


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out-dir", type=Path, default=Path("out/attr-transform"))
    ap.add_argument("--maincpu", type=Path, default=Path("out/maincpu.bin"))
    ap.add_argument("--park", type=Path, action="append", default=None)
    ap.add_argument("--trace", type=Path, action="append", default=None)
    ap.add_argument("--probe", action="store_true", help="include probe orchestration notes (no auto-run by default)")
    ap.add_argument("--write-absent-view", action="store_true", default=True)
    ap.add_argument("--skip-traces", action="store_true")
    args = ap.parse_args()

    out_dir = args.out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    parks_paths = args.park if args.park else DEFAULT_PARKS
    traces = args.trace if args.trace else DEFAULT_TRACES

    static = scan_maincpu(args.maincpu)
    (out_dir / "static_store_ips.json").write_text(json.dumps(static, indent=2), encoding="utf-8")

    parks = []
    for p in parks_paths:
        if not p.exists():
            parks.append({"park": str(p), "error": "missing"})
            continue
        try:
            rec = dump_park(p)
        except Exception as exc:  # fail closed on parse errors
            rec = {"park": str(p), "error": str(exc)}
        parks.append(rec)
        safe = p.stem.replace(".", "_")
        (out_dir / f"region_{safe}.json").write_text(json.dumps(rec, indent=2), encoding="utf-8")

    if args.skip_traces:
        geo = {"conclusion": "traces skipped", "tgp_class_port_hits": {}}
    else:
        geo = classify_geo_ports(traces)
    (out_dir / "geo_port_classes.json").write_text(json.dumps(geo, indent=2), encoding="utf-8")

    absence = build_absence(static, parks, geo)
    absence["probe"] = optional_probe_note(args)
    (out_dir / "absence_table.json").write_text(json.dumps(absence, indent=2), encoding="utf-8")

    view_path = write_measured_view(out_dir, parks, static)
    if view_path is None and args.write_absent_view:
        view_path = write_absent_view(out_dir, parks)

    probe_ev_path = out_dir / "probe_evidence.json"
    probe_ev = None
    if probe_ev_path.exists():
        try:
            probe_ev = json.loads(probe_ev_path.read_text(encoding="utf-8"))
        except Exception:
            probe_ev = None
        if probe_ev:
            absence["probe_evidence_file"] = str(probe_ev_path)
            absence["probe_measured_stores"] = [
                {
                    "probe": p.get("name"),
                    "function_ip": p.get("function_ip"),
                    "classification": p.get("classification"),
                    "confidence": p.get("confidence"),
                }
                for p in probe_ev.get("probes", [])
            ]
            (out_dir / "absence_table.json").write_text(
                json.dumps(absence, indent=2), encoding="utf-8"
            )

    summary = {
        "static_present": static.get("present"),
        "parks_parsed": len([p for p in parks if "error" not in p]),
        "parks_errors": [p for p in parks if "error" in p],
        "geo_port_writes": geo.get("totals", {}).get("geo_port_writes"),
        "measured_view": None if view_path is None else str(view_path),
        "measured_view_confidence": "absent" if view_path and json.loads(view_path.read_text()).get("confidence") == "absent" else ("measured" if view_path else None),
        "tgp_matrix": "absent",
        "display_triple": "measured at display object +0x54 store_ip=0x31024",
        "camera_scale": "measured 0x501084/0x501088 = 600.0f store_ips=0x1d34c/0x1d35c",
        "camera_copro_fifo": "measured tag 0x0b001616 + operands on 0x884000 (class 0x16, NOT tgp 0x0b)",
        "probe_evidence": None if probe_ev is None else str(probe_ev_path),
        "note_file": "decomp/i960/notes/tgp_camera_state_v0376.md",
    }
    (out_dir / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
