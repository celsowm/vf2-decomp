#!/usr/bin/env python3
"""P2 v0377 analyzer — post-probe evidence dig.

Fail-closed. Does not invent a camera matrix.
1. Classify every memory write in probe traces (geo/TGP/aperture/work/other).
2. Scan maincpu for camera-scratch IEEE bits (223.2f / 172.8f) and abs ports.
3. Scan snap geometry/coprocessor regions for identity-like 12-float windows.
4. Emit measured_view.json + port_absence + region dumps + negative evidence.
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from collections import Counter
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, WORK_BASE  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "out" / "attr-transform"
MAINCPU = ROOT / "out" / "maincpu.bin"

IEEE = {
    "6.0f": 0x40C00000,
    "4.7f": 0x40966666,
    "18.5f": 0x41940000,
    "600.0f": 0x44160000,
    "223.2f": 0x435F3333,
    "172.8f": 0x432CCCCD,
    "fifo_copro_arith_tag": 0x0B001616,
}

ABS_PORTS = {
    "0x0050084c": 0x0050084C,
    "0x00501084": 0x00501084,
    "0x00501088": 0x00501088,
    "0x005001e4": 0x005001E4,
    "0x0090e000": 0x0090E000,
    "0x00880000": 0x00880000,
    "0x00980000": 0x00980000,
    "0x00800000": 0x00800000,
    "0x00800010": 0x00800010,
    "0x00804000": 0x00804000,
    "0x00884000": 0x00884000,
}

PORTS_CHECKED = [
    {
        "port": "aperture_buffer",
        "addr": "0x0090e000",
        "method": "disasm display_command_emit@0x31040 + dynamic --set-ip 0x31040 memory-trace + snap cursor 0x5001e4",
        "measured_content": "display triple (6.0f,4.7f,18.5f) + FIFO protocol words only",
        "is_matrix": False,
    },
    {
        "port": "display_object",
        "addr": "*(u32*)0x50084c + 0x00..0x90",
        "method": "snap-parser full dump across parks",
        "measured_content": "triple at +0x54/58/5c; +0x40 flags; +0x60/64 damped state; no 12-float block",
        "is_matrix": False,
    },
    {
        "port": "camera_task",
        "addr": "0x00515400 / live g13",
        "method": "snap dump + camera-path memory-trace",
        "measured_content": "task fields + FIFO readback +0x5c=223.2f +0x60=172.8f; not focus/matrix",
        "is_matrix": False,
    },
    {
        "port": "camera_scale",
        "addr": "0x00501084 / 0x00501088",
        "method": "store IPs 0x1d34c/0x1d35c + memory-trace",
        "measured_content": "600.0f / 600.0f",
        "is_matrix": False,
    },
    {
        "port": "work_ram_matrix_window",
        "addr": "0x00500000..0x00520000",
        "method": "12-float identity-like scan on parks + camera-path write inventory",
        "measured_content": "no guest store of 12-float matrix; 0x50a0e0 is unit profile table",
        "is_matrix": False,
    },
    {
        "port": "tgp_function_port",
        "addr": "0x00880000",
        "method": "memory-trace all probes + ROM absolute-immediate scan",
        "measured_content": "0 dynamic writes; ROM hits unaligned/data-like",
        "is_matrix": False,
    },
    {
        "port": "tgp_upload_ctl",
        "addr": "0x00980000",
        "method": "memory-trace + ROM lda @ 0x0f0c/0x2ec4",
        "measured_content": "boot/coprocessor control only; 0 payload matrix writes in probes",
        "is_matrix": False,
    },
    {
        "port": "geo_control",
        "addr": "0x00800000",
        "method": "memory-trace + model2a map + MAME suggestion",
        "measured_content": "0 writes of matrix payload; object_submit may touch g10-relative geo object word",
        "is_matrix": False,
    },
    {
        "port": "geo_program",
        "addr": "0x00804000",
        "method": "memory-trace + ROM immediate scan",
        "measured_content": "0 writes; 0 absolute immediates in maincpu.bin",
        "is_matrix": False,
    },
    {
        "port": "copro_fifo",
        "addr": "0x00884000",
        "method": "camera-path memory-trace",
        "measured_content": "arith tags 0x0b001616/0x12002424/0x15802b2b/0x07800f0f + operands",
        "is_matrix": False,
    },
    {
        "port": "fifo_geo_stream_class_09_0b_0c",
        "addr": "stream via geo_w/geo_prg_w",
        "method": "v0375/v0376 protocol filter (not relitigated)",
        "measured_content": "class 09/0b/0c ABSENT after protocol filter",
        "is_matrix": False,
    },
]


def f32(bits: int) -> float | None:
    try:
        return struct.unpack("<f", struct.pack("<I", bits & 0xFFFFFFFF))[0]
    except Exception:
        return None


def classify_addr(addr: int) -> str:
    if 0x00500000 <= addr < 0x00600000:
        return "work_ram"
    if 0x0090E000 <= addr < 0x0090E000 + 0x400:
        return "aperture"
    if 0x00880000 <= addr < 0x00882000:
        return "tgp_function_port"
    if 0x00980000 <= addr < 0x00980100:
        return "tgp_upload_ctl"
    if 0x00884000 <= addr < 0x00888000:
        return "copro_fifo"
    if 0x00800000 <= addr < 0x00804000:
        return "geo_control"
    if 0x00804000 <= addr < 0x00808000:
        return "geo_program"
    if 0x00800000 <= addr < 0x00890000:
        return "geo_copro_other"
    if 0x01800000 <= addr < 0x01900000:
        return "backup_or_table"
    return "other"


def scan_pattern(rom: bytes, pat: int) -> list[dict[str, Any]]:
    needle = struct.pack("<I", pat)
    hits = []
    start = 0
    while True:
        j = rom.find(needle, start)
        if j < 0:
            break
        prev = struct.unpack_from("<I", rom, j - 4)[0] if j >= 4 else 0
        prev_hi = (prev >> 24) & 0xFF
        kind = "data_like"
        if j % 4 == 0 and prev_hi == 0x8C:
            kind = "lda_imm"
        elif j % 4 == 0:
            kind = "aligned_word"
        else:
            kind = "unaligned"
        hits.append(
            {
                "off": f"0x{j:x}",
                "kind": kind,
                "prev": f"0x{prev:08x}",
                "prev_hi": f"0x{prev_hi:02x}",
            }
        )
        start = j + 1
    return hits


def analyze_trace(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {"path": str(path), "error": "missing", "writes": []}
    text = path.read_text(encoding="utf-8", errors="replace")
    writes = []
    geo_range = []
    aperture = []
    for line in text.splitlines():
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            j = json.loads(line)
        except Exception:
            continue
        if j.get("type") != "memory":
            continue
        kind = j.get("kind")
        addr = j.get("address", 0)
        try:
            bs = bytes.fromhex(j.get("bytes", "00"))
            val = int.from_bytes(bs[:4], "little")
        except Exception:
            val = 0
        rec = {
            "kind": kind,
            "step": j.get("step"),
            "addr": f"0x{addr:08x}",
            "addr_int": addr,
            "value": f"0x{val:08x}" if kind == "write" else None,
            "f32": f32(val) if kind == "write" else None,
            "class": classify_addr(addr),
        }
        writes.append(rec)
        if kind == "write":
            if 0x00800000 <= addr < 0x00890000 or 0x00980000 <= addr < 0x00980100:
                geo_range.append(rec)
            if 0x0090E000 <= addr < 0x0090E200:
                aperture.append(rec)

    wcount = Counter(r["class"] for r in writes if r["kind"] == "write")
    rcount = Counter(r["class"] for r in writes if r["kind"] == "read")
    return {
        "path": str(path),
        "n_records": len(writes),
        "n_writes": sum(1 for r in writes if r["kind"] == "write"),
        "n_reads": sum(1 for r in writes if r["kind"] == "read"),
        "write_class": dict(wcount),
        "read_class": dict(rcount),
        "geo_copro_writes": geo_range,
        "aperture_writes": aperture[:32],
        "all_write_addrs_sample": [
            r for r in writes if r["kind"] == "write"
        ][:80],
    }


def looks_float_word(val: int) -> bool:
    exp = (val >> 23) & 0xFF
    return 0x38 <= exp <= 0x48 or val in (0, 0x3F800000)


def scan_matrix_windows(words: list[int], base_label: str) -> dict[str, Any]:
    candidates = []
    rejected = []
    n = len(words)
    if n < 12:
        return {"base": base_label, "candidates": [], "rejected": []}
    for i in range(0, n - 11):
        window = words[i : i + 12]
        floatish = sum(1 for w in window if looks_float_word(w))
        ones = sum(1 for w in window if w == 0x3F800000)
        zeros = sum(1 for w in window if w == 0)
        if floatish < 10:
            continue
        diag = [window[0], window[5], window[10]]
        score = sum(1 for d in diag if d == 0x3F800000)
        # unit-length row heuristic: |row0|~1 if first 3 floats
        row_norms = []
        for r0 in (0, 4, 8):
            triple = window[r0 : r0 + 3]
            s = 0.0
            ok = True
            for w in triple:
                fl = f32(w)
                if fl is None:
                    ok = False
                    break
                s += fl * fl
            row_norms.append(s**0.5 if ok else None)
        rec = {
            "off": f"0x{i*4:05x}",
            "addr": f"{base_label}+0x{i*4:05x}",
            "floatish": floatish,
            "ones": ones,
            "zeros": zeros,
            "diag_score": score,
            "row_norms": row_norms,
            "window": [f"0x{w:08x}" for w in window],
            "window_f32": [f32(w) for w in window],
        }
        if ones == 12:
            rec["reject_reason"] = "twelve_consecutive_1.0f_unit_table"
            rejected.append(rec)
        elif zeros == 12:
            rec["reject_reason"] = "all_zero"
            rejected.append(rec)
        elif score >= 2 and zeros >= 4 and ones <= 6:
            candidates.append(rec)
        elif floatish >= 10:
            rec["reject_reason"] = "floatish_but_not_identity_pattern"
            rejected.append(rec)
    candidates.sort(key=lambda c: (-c["diag_score"], -c["zeros"]))
    return {
        "base": base_label,
        "candidates": candidates[:6],
        "rejected": rejected[:8],
    }


def dump_park(path: Path) -> dict[str, Any]:
    snap = parse_snap(path)
    regions = snap["regions"]
    work = regions.get("work-ram", b"")
    geom = regions.get("geometry", b"")
    copro = regions.get("copro-port", b"")
    copro_ctl = regions.get("copro-control", b"")
    buf = regions.get("buffer-ram", b"")

    def wu32(addr: int) -> int | None:
        off = addr - WORK_BASE
        if off < 0 or off + 4 > len(work):
            return None
        return struct.unpack_from("<I", work, off)[0]

    def words(blob: bytes, limit: int) -> list[int]:
        out = []
        for i in range(0, min(len(blob), limit) - 3, 4):
            out.append(struct.unpack_from("<I", blob, i)[0])
        return out

    dptr = wu32(0x0050084C)
    cursor = wu32(0x005001E4)
    display = []
    if dptr is not None and 0x00500000 <= dptr < 0x00500000 + len(work):
        for off in range(0, 0x90, 4):
            v = wu32(dptr + off)
            if v is None:
                break
            display.append(
                {
                    "off": f"+0x{off:02x}",
                    "addr": f"0x{dptr+off:08x}",
                    "bits": f"0x{v:08x}",
                    "f32": f32(v),
                }
            )

    cam = []
    for off in range(0, 0x1C0, 4):
        v = wu32(0x00515400 + off)
        if v is None:
            break
        if v != 0:
            cam.append(
                {
                    "off": f"+0x{off:03x}",
                    "addr": f"0x{0x515400+off:08x}",
                    "bits": f"0x{v:08x}",
                    "f32": f32(v),
                }
            )

    scan_limit = min(len(work), 0x20000)
    work_words = words(work, scan_limit)
    return {
        "park": str(path),
        "ip": f"0x{snap['ip']:08x}",
        "executed": snap["executed"],
        "cursor_0x5001e4": None if cursor is None else f"0x{cursor:08x}",
        "display_ptr": None if dptr is None else f"0x{dptr:08x}",
        "display_object_words": display,
        "camera_scale": {
            "0x501084": (lambda v: None if v is None else {"bits": f"0x{v:08x}", "f32": f32(v)})(wu32(0x00501084)),
            "0x501088": (lambda v: None if v is None else {"bits": f"0x{v:08x}", "f32": f32(v)})(wu32(0x00501088)),
        },
        "camera_task_nonzero": cam,
        "geometry_region_bytes": len(geom),
        "copro_port_bytes": len(copro),
        "copro_control_bytes": len(copro_ctl),
        "buffer_ram_bytes": len(buf),
        "geometry_matrix_like": scan_matrix_windows(words(geom, 0x8000), "geometry"),
        "copro_port_matrix_like": scan_matrix_windows(words(copro, 0x2000), "copro-port"),
        "buffer_matrix_like": scan_matrix_windows(words(buf, 0x4000), "buffer-ram"),
        "work_matrix_like": scan_matrix_windows(work_words, "work-ram"),
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, default=OUT)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    print("[1] ROM IEEE + abs port scan")
    static: dict[str, Any] = {"path": str(MAINCPU), "patterns": {}, "abs_ports": {}}
    if MAINCPU.exists():
        rom = MAINCPU.read_bytes()
        static["size"] = len(rom)
        for name, pat in IEEE.items():
            static["patterns"][name] = scan_pattern(rom, pat)
        for name, val in ABS_PORTS.items():
            static["abs_ports"][name] = scan_pattern(rom, val)
    (args.out / "static_ieee_v0377.json").write_text(
        json.dumps(static, indent=2), encoding="utf-8"
    )

    print("[2] probe trace analysis")
    trace_paths = sorted(args.out.glob("probe_*_v0377.jsonl"))
    traces = [analyze_trace(p) for p in trace_paths]
    (args.out / "probe_trace_analysis_v0377.json").write_text(
        json.dumps(traces, indent=2), encoding="utf-8"
    )

    print("[3] park region dumps")
    parks = [
        ROOT / "out" / "attr-long" / "long-29.vf2snap",
        ROOT / "out" / "attr-logo" / "emit31040.vf2snap",
        ROOT / "out" / "attr-v0372" / "boot-sel03-fifo.vf2snap",
        ROOT / "out" / "attr-v0372" / "boot-sel09-fifo.vf2snap",
        ROOT / "out" / "attr-fs" / "all0-ready1.vf2snap",
        ROOT / "out" / "sixth-fresh.vf2snap",
    ]
    park_dumps = []
    any_matrix = False
    matrix_hits = []
    for p in parks:
        if not p.exists():
            park_dumps.append({"park": str(p), "error": "missing"})
            continue
        d = dump_park(p)
        park_dumps.append(d)
        name = p.stem
        (args.out / f"region_park_{name}_v0377.json").write_text(
            json.dumps(d, indent=2), encoding="utf-8"
        )
        for key in (
            "work_matrix_like",
            "geometry_matrix_like",
            "copro_port_matrix_like",
            "buffer_matrix_like",
        ):
            for c in (d.get(key) or {}).get("candidates") or []:
                if c.get("diag_score", 0) >= 2:
                    any_matrix = True
                    matrix_hits.append({"park": str(p), "key": key, "cand": c})
    (args.out / "region_parks_all_v0377.json").write_text(
        json.dumps({"parks": park_dumps}, indent=2), encoding="utf-8"
    )

    print("[4] negative evidence + measured_view")
    geo_writes_all = []
    aperture_writes_all = []
    for t in traces:
        geo_writes_all.extend(t.get("geo_copro_writes") or [])
        aperture_writes_all.extend(t.get("aperture_writes") or [])

    cam_scratch_rom = {
        "223.2f_0x435f3333": static["patterns"].get("223.2f", []),
        "172.8f_0x432ccccd": static["patterns"].get("172.8f", []),
        "note": (
            "Dynamic writers of 223.2f/172.8f are the camera path FIFO round-trip "
            "lda imm -> st FIFO -> ld FIFO -> st task+0x5c/0x60 at IPs "
            "0x1d488/0x1d498 and 0x1d4b4/0x1d4c4. ROM lda sites are the same "
            "function immediates; no independent matrix-store site uses these bits."
        ),
    }

    negative_evidence = {
        "question": (
            "Measured geometry matrix / focus / projection state OUTSIDE FIFO "
            "protocol tags (class 09/0b/0c already absent v0375/v0376)?"
        ),
        "answer": "absent",
        "ports_checked": PORTS_CHECKED,
        "dynamic": {
            "probe_traces": [t.get("path") for t in traces],
            "geo_copro_writes_all_probes": geo_writes_all[:40],
            "aperture_writes_all_probes": aperture_writes_all[:40],
            "camera_path_after_1d4c4_stores": [
                "work-RAM task/profile fields only (0x50a014, 0x50a024, 0x500174-180, 0x50109c/a0/e8/ea)",
                "FIFO copro arith tags 0x12002424 + operands",
                "NO store to 0x008000xx geo matrix registers",
                "NO store to 0x00880000 TGP function port",
                "NO store of 12-float matrix to any measured port",
            ],
            "display_command_emit_consumer": (
                "Writes display triple to 0x0090e000[cursor]; then call 0x7c60 "
                "object-submit helper. Helper writes FIFO protocol 0x1a003434, "
                "object-table pointer via 0x020e0004[g0*16], and st/stq object "
                "word to g10+0x10 / FIFO. g10 is geo base 0x00800000 in recovered "
                "register state — that is an OBJECT SUBMIT word, not a 12-float "
                "TGP matrix. No path serializes transformed verts or 3x4 to geo/TGP."
            ),
            "display_transform_update_0x311b8": (
                "Damps/lerps display triple and stores back to display+0x54 via "
                "stt; FIFO protocol 0x13802727 + operands; also stt display+0x60. "
                "Still Work-RAM display-state, not TGP matrix delivery."
            ),
        },
        "static": {
            "223.2_172.8_sites": cam_scratch_rom,
            "abs_port_rom_hits": static.get("abs_ports", {}),
            "display_triple_lda_sites": {
                "6.0f": ["0x3100c", "0x311e8", "0x1d774", "..."],
                "4.7f": ["0x31014", "0x311f0"],
                "18.5f": ["0x3101c", "0x311f8"],
                "600.0f": ["0x1d344", "0x1d354"],
            },
        },
        "snap_parser": {
            "parks_scanned": [p["park"] for p in park_dumps if "error" not in p],
            "identity_like_matrix_candidates": matrix_hits,
            "rejected_false_positive_0x50a0e0": (
                "twelve consecutive 1.0f unit-profile table, not identity 3x4"
            ),
        },
        "already_absent_not_relitigated": [
            "FIFO/geo stream class 0x09 focus",
            "FIFO/geo stream class 0x0b 3x4 matrix",
            "FIFO/geo stream class 0x0c translate",
            "FIFO/geo stream class 0x07 geometry_mode (residual IEEE 266f only)",
        ],
        "fail_closed": (
            "Do NOT promote (6.0,4.7,18.5) to view matrix; do NOT promote "
            "0x0b001616 to matrix opcode; do NOT promote 223.2/172.8 to focus; "
            "do NOT promote 0x50a0e0 unit table to identity matrix; do NOT "
            "invent projection from hybrid.c / fa_camera recovery."
        ),
    }

    measured_view = {
        "version": "v0377-p2",
        "matrix": None,
        "focus_x": None,
        "focus_y": None,
        "geometry_mode": None,
        "confidence": "absent",
        "ports_checked": [p["port"] + "@" + p["addr"] for p in PORTS_CHECKED],
        "ports_checked_detail": PORTS_CHECKED,
        "negative_evidence": negative_evidence,
        "related_measured_work_ram": {
            "display_triple": {
                "addr": "*(u32*)0x50084c + 0x54/58/5c",
                "values": ["0x40c00000 (6.0f)", "0x40966666 (4.7f)", "0x41940000 (18.5f)"],
                "store_ip": "0x00031024",
                "function": "display_transform_defaults@0x31004",
                "classification": "work_ram_display_state_not_tgp_matrix",
            },
            "camera_scale": {
                "addr": ["0x00501084", "0x00501088"],
                "values": ["0x44160000 (600.0f)"],
                "store_ips": ["0x0001d34c", "0x0001d35c"],
                "function": "fa_camera_initialize_prefix@0x1d320",
                "classification": "work_ram_camera_scale_not_projection",
            },
            "camera_task_scratch": {
                "addr": "task+0x5c / +0x60",
                "values": ["0x435f3333 (223.2f)", "0x432ccccd (172.8f)"],
                "store_ips": ["0x0001d498", "0x0001d4c4"],
                "source": "FIFO copro arith round-trip tag 0x0b001616 class 0x16",
                "classification": "camera_task_scratch_not_focus",
            },
            "fifo_copro_arith_tag": {
                "word": "0x0b001616",
                "tgp_class_bits": "0x16",
                "classification": "NOT_tgp_class_0b_matrix_opcode",
            },
        },
        "consumer_of_aperture": (
            "display_command_emit@0x31040 -> 0x0090e000[cursor] triple only; "
            "call 0x7c60 object-submit (geo object word + FIFO), not matrix."
        ),
        "tgp_model_note": (
            "src/hardware/tgp.c models geometry_matrix identity + geometry_write_matrix "
            "as a 16-float 4x4 filled by GEO STREAM opcodes. Guest never delivered "
            "those opcodes in measured windows; model stays identity passthrough."
        ),
    }

    (args.out / "measured_view.json").write_text(
        json.dumps(measured_view, indent=2), encoding="utf-8"
    )
    (args.out / "port_absence_v0377.json").write_text(
        json.dumps(
            {
                "version": "v0377-p2",
                "ports": PORTS_CHECKED,
                "overall": {
                    "measured_tgp_matrix_on_alternate_port": False,
                    "measured_focus_on_alternate_port": False,
                    "measured_geometry_mode_on_alternate_port": False,
                    "confidence": "absent",
                },
                "negative_evidence_ref": "out/attr-transform/measured_view.json",
            },
            indent=2,
        ),
        encoding="utf-8",
    )

    summary = {
        "confidence": "absent",
        "matrix": None,
        "focus_x": None,
        "focus_y": None,
        "any_identity_like_matrix": any_matrix,
        "matrix_hits": matrix_hits,
        "geo_writes_count_all_traces": len(geo_writes_all),
        "aperture_writes_count_all_traces": len(aperture_writes_all),
        "rom_223_2_hits": len(static["patterns"].get("223.2f", [])),
        "rom_172_8_hits": len(static["patterns"].get("172.8f", [])),
        "note_file": "decomp/i960/notes/matrix_ports_p2_v0377.md",
        "measured_view": "out/attr-transform/measured_view.json",
    }
    (args.out / "summary_v0377.json").write_text(
        json.dumps(summary, indent=2), encoding="utf-8"
    )
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
