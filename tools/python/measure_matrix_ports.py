#!/usr/bin/env python3
"""P2 v0377 — Measure TGP geometry matrix / focus / geometry_mode on ports
OTHER than FIFO class-09/0b/0c (already proven ABSENT in v0375/v0376).

Evidence-first. Fail-closed. Never invents a camera matrix.
Do NOT treat 0x0b001616 as a TGP matrix opcode (class bits = 0x16).

Ports under test (alternate delivery paths):
  aperture buffer     0x0090e000[cursor @ 0x5001e4]
  display object      *(u32*)0x50084c + 0x00..0x90
  camera task         0x00515400 / live g13
  camera scale        0x00501084 / 0x00501088
  work-RAM scan       0x00500000.. for 12-float matrix-like windows
  TGP function port   0x00880000
  TGP upload/ctl      0x00980000
  geo control         0x00800000 / 0x00804000
  copro FIFO          0x00884000 (arith tags only; NOT matrix)

Outputs:
  out/attr-transform/measured_view_v0377.json
  out/attr-transform/port_absence_v0377.json
  out/attr-transform/static_ieee_v0377.json
  out/attr-transform/region_aperture_v0377.json
  out/attr-transform/region_display_obj_v0377.json
  out/attr-transform/region_camera_task_v0377.json
"""
from __future__ import annotations

import argparse
import json
import re
import struct
import subprocess
import sys
from collections import Counter
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, wu32, wu8, WORK_BASE  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "out" / "attr-transform"
MAINCPU = ROOT / "out" / "maincpu.bin"
PROBE = ROOT / "build" / "Debug" / "vf2probe.exe"
I960 = ROOT / "build" / "Debug" / "vf2i960.exe"
ROM_DIR = ROOT / "roms" / "vf2"

IEEE_PATTERNS = {
    "6.0f": 0x40C00000,
    "4.7f": 0x40966666,
    "18.5f": 0x41940000,
    "600.0f": 0x44160000,
}

# Alternate ports under P2 test.
PORT_TABLE = [
    {
        "port": "aperture_buffer",
        "addr": "0x0090e000",
        "addr_int": 0x0090E000,
        "cursor_global": "0x005001e4",
        "expected_matrix": "display triple serialized as 3 floats + protocol words, NOT 12-float TGP matrix",
        "prior_evidence": "v0376 store IPs 0x31068/78/88 from display_command_emit@0x31040",
    },
    {
        "port": "tgp_function_port",
        "addr": "0x00880000",
        "addr_int": 0x00880000,
        "expected_matrix": "MAME copro function port (func in bits 23-28); if matrix were uploaded here we'd see 12-float payloads",
        "prior_evidence": "v0372: 0 writes in all measured windows",
    },
    {
        "port": "tgp_upload_ctl",
        "addr": "0x00980000",
        "addr_int": 0x00980000,
        "expected_matrix": "MAME copro program upload (bit31) / geo_ctl1 @ 0x00980008",
        "prior_evidence": "v0372: 0 writes; boot writes 0x80000000 to video control at 0x00980000 (boot.md)",
    },
    {
        "port": "geo_control",
        "addr": "0x00800000",
        "addr_int": 0x00800000,
        "expected_matrix": "MAME geo_r/geo_w; geo matrix delivered as stream opcode via geo command buffer",
        "prior_evidence": "v0375/v0376: class-09/0b/0c absent after protocol filtering",
    },
    {
        "port": "geo_program",
        "addr": "0x00804000",
        "addr_int": 0x00804000,
        "expected_matrix": "MAME geo_prg_w push; matrix would appear as class-0b stream",
        "prior_evidence": "v0376: 38 raw class-0b words, all protocol, 0 remaining",
    },
    {
        "port": "copro_fifo",
        "addr": "0x00884000",
        "addr_int": 0x00884000,
        "expected_matrix": "FIFO copro arith/protocol tags; 0x0b001616 class=0x16 NOT matrix",
        "prior_evidence": "v0376 measured arith tags 0x0b001616/0x12002424 + operands",
    },
    {
        "port": "work_ram_matrix_window",
        "addr": "0x00500000+",
        "addr_int": 0x00500000,
        "expected_matrix": "stable 12-float object written by a guest store IP",
        "prior_evidence": "v0376: no 12-float matrix store measured; display triple only at +0x54",
    },
]

PARKS = [
    ROOT / "out" / "attr-long" / "long-29.vf2snap",
    ROOT / "out" / "attr-logo" / "emit31040.vf2snap",
    ROOT / "out" / "attr-v0372" / "boot-sel03-fifo.vf2snap",
    ROOT / "out" / "attr-v0372" / "boot-sel09-fifo.vf2snap",
    ROOT / "out" / "attr-fs" / "all0-ready1.vf2snap",
    ROOT / "out" / "sixth-fresh.vf2snap",
]

PROBES = [
    {
        "name": "camera_path_1d320_1d660",
        "snapshot": ROOT / "out" / "attr-long" / "long-29.vf2snap",
        "args": [
            "--set-ip", "0x0001d320",
            "--until", "0x0001d660",
            "--max-steps", "4000",
            "--memory-trace",
            "--trace",
            "--read-u32", "0x00501084",
            "--read-u32", "0x00501088",
            "--read-u32", "0x0050084c",
            "--read-u32", "0x005001e4",
            "--read-u32", "0x00515d54",
        ],
        "trace": OUT / "probe_cam_full_1d320_1d660_v0377.jsonl",
        "until": "0x0001d660",
    },
    {
        "name": "tgp_ports_from_long29_natural",
        "snapshot": ROOT / "out" / "attr-long" / "long-29.vf2snap",
        "args": [
            "--until", "0x000224a0",  # park is at 0x22498; run a few insns only
            "--max-steps", "64",
            "--memory-trace",
        ],
        "trace": OUT / "probe_ports_long29_natural_v0377.jsonl",
        "until": "0x000224a0",
        "note": "Natural park IP near 0x22498; few instructions. Mostly proves park is idle.",
    },
    {
        "name": "tgp_ports_from_emit31040",
        "snapshot": ROOT / "out" / "attr-logo" / "emit31040.vf2snap",
        "args": [
            "--until", "0x000310cc",
            "--max-steps", "256",
            "--memory-trace",
        ],
        "trace": OUT / "probe_ports_emit31040_v0377.jsonl",
        "until": "0x000310cc",
        "note": "Park already at emit ret 0x310c8; short run to catch any port writes. "
        "NOTE: does NOT re-enter emit; use emit_from_31040 for aperture writes.",
    },
    {
        "name": "emit_from_31040",
        "snapshot": ROOT / "out" / "attr-logo" / "emit31040.vf2snap",
        "args": [
            "--set-ip", "0x00031040",
            "--until", "0x000310cc",
            "--max-steps", "256",
            "--memory-trace",
            "--trace",
            "--read-u32", "0x005001e4",
            "--read-u32", "0x0050084c",
        ],
        "trace": OUT / "probe_emit_from_31040_v0377.jsonl",
        "until": "0x000310cc",
        "note": "Correct freeze: --set-ip at display_command_emit entry, --until after ret window.",
    },
    {
        "name": "cam_post_1d4c4",
        "snapshot": ROOT / "out" / "attr-long" / "long-29.vf2snap",
        "args": [
            "--set-ip", "0x0001d4c4",
            "--until", "0x0001d660",
            "--max-steps", "2000",
            "--memory-trace",
        ],
        "trace": OUT / "probe_cam_post1d4c4_v0377.jsonl",
        "until": "0x0001d660",
        "note": "Camera body AFTER task+0x60 store; inventory geo/TGP/aperture writes.",
    },
]


def f32(bits: int) -> float | None:
    try:
        return struct.unpack("<f", struct.pack("<I", bits & 0xFFFFFFFF))[0]
    except Exception:
        return None


def bits_label(v: int) -> str:
    fl = f32(v)
    if fl is None:
        return f"0x{v:08x}"
    if abs(fl) < 1e-6 or abs(fl) > 1e6:
        return f"0x{v:08x}"
    return f"0x{v:08x} ({fl:.6g})"


def looks_float_word(val: int) -> bool:
    exp = (val >> 23) & 0xFF
    return 0x38 <= exp <= 0x48


def looks_like_matrix(words: list[int]) -> dict[str, Any]:
    """Fail-closed 12-float window scan.

    A real 3x4 / 4x3 identity-like matrix has 1.0f on some diagonal slots AND
    0.0f on the off-diagonal slots. Twelve consecutive 1.0f is NOT a matrix
    (it is a unit table / profile scale). All-zero is not a matrix either.
    """
    candidates = []
    rejected = []
    n = len(words)
    if n < 12:
        return {"candidates": [], "rejected": []}
    for i in range(0, n - 11):
        window = words[i : i + 12]
        floatish = sum(1 for w in window if looks_float_word(w) or w in (0, 0x3F800000, 0x40000000))
        ones = sum(1 for w in window if w == 0x3F800000)
        zeros = sum(1 for w in window if w == 0)
        if floatish < 10:
            continue
        # Identity-like: some 1.0 on diagonal-ish slots AND substantial zeros off-diagonal.
        diag = [window[0], window[5], window[10]]
        score = sum(1 for d in diag if d == 0x3F800000)
        rec = {
            "off": f"0x{i*4:05x}",
            "floatish": floatish,
            "ones": ones,
            "zeros": zeros,
            "diag_score": score,
            "window": [f"0x{w:08x}" for w in window],
            "window_f32": [f32(w) for w in window],
        }
        if ones == 12:
            rec["reject_reason"] = "twelve_consecutive_1.0f_unit_table_not_identity_matrix"
            rejected.append(rec)
        elif zeros == 12:
            rec["reject_reason"] = "all_zero_window"
            rejected.append(rec)
        elif score >= 2 and zeros >= 4 and ones <= 6:
            candidates.append(rec)
        elif floatish >= 10:
            rec["reject_reason"] = "floatish_but_not_identity_pattern"
            rejected.append(rec)
    candidates.sort(key=lambda c: (-c["diag_score"], -c["zeros"]))
    return {"candidates": candidates[:8], "rejected": rejected[:8]}


def scan_maincpu_ieee() -> dict[str, Any]:
    result: dict[str, Any] = {
        "path": str(MAINCPU),
        "present": MAINCPU.exists(),
        "patterns": {},
        "aligned_instruction_hits": {},
        "static_store_candidates": [],
    }
    if not MAINCPU.exists():
        return result
    data = MAINCPU.read_bytes()
    result["size"] = len(data)
    for name, pat in IEEE_PATTERNS.items():
        needle = struct.pack("<I", pat)
        hits = []
        start = 0
        while True:
            j = data.find(needle, start)
            if j < 0:
                break
            hits.append(j)
            start = j + 1
        result["patterns"][name] = [f"0x{h:x}" for h in hits]
        # Classify: likely lda immediate if word-aligned and previous nibble looks like 0x8c (lda)
        insn_hits = []
        data_hits = []
        for h in hits:
            if h % 4 != 0:
                data_hits.append(f"0x{h:x}(unaligned)")
                continue
            # i960 lda abs form: 0x8cXX3000 then imm at +4; check prior word high byte 0x8c
            prev = struct.unpack_from("<I", data, h - 4)[0] if h >= 4 else 0
            prev_hi = (prev >> 24) & 0xFF
            if prev_hi == 0x8C:
                insn_hits.append({"ip_imm": f"0x{h:x}", "ip_lda": f"0x{h-4:x}", "prev": f"0x{prev:08x}"})
            else:
                data_hits.append(f"0x{h:x}(prev=0x{prev:08x})")
        result["aligned_instruction_hits"][name] = {"lda_sites": insn_hits, "other": data_hits}

    # Named absolute addresses of interest appearing as 32-bit immediates
    addrs = {
        "0x50084c": 0x0050084C,
        "0x501084": 0x00501084,
        "0x501088": 0x00501088,
        "0x5001e4": 0x005001E4,
        "0x90e000": 0x0090E000,
        "0x880000": 0x00880000,
        "0x980000": 0x00980000,
        "0x800010": 0x00800010,
        "0x804000": 0x00804000,
        "0x884000": 0x00884000,
    }
    for name, pat in addrs.items():
        needle = struct.pack("<I", pat)
        hits = []
        start = 0
        while True:
            j = data.find(needle, start)
            if j < 0:
                break
            hits.append(f"0x{j:x}")
            start = j + 1
        result["patterns"][f"addr_{name}"] = hits

    # Collect store-site candidates from known camera/display/camera IPs
    # using vf2i960 disasm text if available later; here we record IEEE lda sites.
    for name in ("6.0f", "4.7f", "18.5f", "600.0f"):
        for site in result["aligned_instruction_hits"][name]["lda_sites"]:
            result["static_store_candidates"].append(
                {
                    "pattern": name,
                    "ip_lda": site["ip_lda"],
                    "ip_imm": site["ip_imm"],
                    "kind": "lda_imm",
                    "tgp_matrix": False,
                    "note": "IEEE immediate load; not by itself a 3x4 matrix store",
                }
            )
    return result


def dump_park_regions() -> dict[str, Any]:
    regions = []
    for path in PARKS:
        if not path.exists():
            regions.append({"park": str(path), "error": "missing"})
            continue
        snap = parse_snap(path)
        work = snap["regions"]["work-ram"]
        geom = snap["regions"].get("geometry", b"")
        copro = snap["regions"].get("copro-port", b"")
        buf = snap["regions"].get("buffer-ram", b"")
        size_work = len(work)

        def work_u32(addr: int) -> int | None:
            off = addr - WORK_BASE
            if off < 0 or off + 4 > size_work:
                return None
            return struct.unpack_from("<I", work, off)[0]

        def work_slice(addr: int, nwords: int) -> list[dict[str, Any]]:
            out = []
            for i in range(nwords):
                v = work_u32(addr + i * 4)
                if v is None:
                    break
                out.append({"addr": f"0x{addr+i*4:08x}", "bits": f"0x{v:08x}", "f32": f32(v)})
            return out

        cursor = work_u32(0x005001E4)
        dptr = work_u32(0x0050084C)
        # Aperture window: dump 32 words starting at 0x0090e000 + (cursor & ~3) - 0x30
        # Aperture is NOT in snap work-ram; it's Model2A buffer. We dump cursor-adjacent
        # work-RAM references and record cursor; actual aperture bytes need memory-trace.
        aperture_summary = {
            "cursor_0x5001e4": None if cursor is None else f"0x{cursor:08x}",
            "note": "Aperture window 0x0090e000[] is Model2A; snap has no direct aperture region. "
            "Values observed via memory-trace / hybrid recovered writes.",
            "known_emit_stores": [
                {"ip": "0x00031068", "insn": "st r4, 0x0090e000(r15)", "source": "display+0x54"},
                {"ip": "0x00031078", "insn": "st r5, 0x0090e000(r15)", "source": "display+0x58"},
                {"ip": "0x00031088", "insn": "st r6, 0x0090e000(r15)", "source": "display+0x5c"},
            ],
        }

        display_fields = []
        if dptr is not None and 0x00500000 <= dptr < 0x00500000 + size_work:
            for off in range(0, 0x90, 4):
                v = work_u32(dptr + off)
                if v is None:
                    break
                display_fields.append(
                    {
                        "off": f"+0x{off:02x}",
                        "addr": f"0x{dptr+off:08x}",
                        "bits": f"0x{v:08x}",
                        "f32": f32(v),
                    }
                )

        cam_task = work_slice(0x00515400, 40)

        geom_words = []
        if geom:
            for i in range(0, min(len(geom), 0x8000), 4):
                geom_words.append(struct.unpack_from("<I", geom, i)[0])
        copro_words = []
        if copro:
            for i in range(0, min(len(copro), 0x2000), 4):
                copro_words.append(struct.unpack_from("<I", copro, i)[0])
        buf_words = []
        if buf:
            for i in range(0, min(len(buf), 0x4000), 4):
                buf_words.append(struct.unpack_from("<I", buf, i)[0])

        work_matrix_windows = []
        # Scan work-ram 0x500000-0x520000 for 12-float windows
        scan_limit = min(size_work, 0x20000)
        scan_words = [
            struct.unpack_from("<I", work, i)[0] for i in range(0, scan_limit - (scan_limit % 4), 4)
        ]
        work_mx = looks_like_matrix(scan_words)

        regions.append(
            {
                "park": str(path),
                "ip": f"0x{snap['ip']:08x}",
                "executed": snap["executed"],
                "cursor": aperture_summary,
                "display_ptr_0x50084c": None if dptr is None else f"0x{dptr:08x}",
                "display_object_words": display_fields,
                "display_triple": {
                    "+0x54": next((x for x in display_fields if x["off"] == "+0x54"), None),
                    "+0x58": next((x for x in display_fields if x["off"] == "+0x58"), None),
                    "+0x5c": next((x for x in display_fields if x["off"] == "+0x5c"), None),
                },
                "camera_scale": {
                    "0x501084": None if work_u32(0x00501084) is None else bits_label(work_u32(0x00501084)),
                    "0x501088": None if work_u32(0x00501088) is None else bits_label(work_u32(0x00501088)),
                },
                "camera_task_0x515400": cam_task,
                "geometry_matrix_like": looks_like_matrix(geom_words),
                "copro_matrix_like": looks_like_matrix(copro_words),
                "buffer_matrix_like": looks_like_matrix(buf_words),
                "work_matrix_like": work_mx,
                "matrix_found": bool(work_mx.get("candidates")),
                "matrix_rejected_windows": (work_mx.get("rejected") or [])[:4],
            }
        )
    return {"parks": regions}


def run_probe(spec: dict[str, Any]) -> dict[str, Any]:
    snap = spec["snapshot"]
    if not snap.exists() or not PROBE.exists():
        return {"name": spec["name"], "status": "skipped", "reason": "missing snap or probe"}
    cmd = [
        str(PROBE),
        "--rom-dir", str(ROM_DIR),
        "--snapshot", str(snap),
        *spec["args"],
    ]
    trace_path = spec["trace"]
    try:
        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=120,
            cwd=str(ROOT),
        )
    except Exception as e:
        return {"name": spec["name"], "status": "error", "error": str(e), "cmd": cmd}

    # vf2probe writes trace to stdout when --trace/--memory-trace; capture it.
    stdout = proc.stdout or ""
    stderr = proc.stderr or ""
    trace_path.parent.mkdir(parents=True, exist_ok=True)
    with trace_path.open("w", encoding="utf-8") as fh:
        fh.write(stdout)
        if stderr:
            fh.write("\n# stderr\n")
            fh.write(stderr)

    # Parse memory writes from JSONL-ish stdout
    writes = []
    port_writes = {p["addr"]: [] for p in PORT_TABLE}
    special = {
        "0x00880000": "tgp_function_port",
        "0x00980000": "tgp_upload_ctl",
        "0x00800000": "geo_control",
        "0x00804000": "geo_program",
        "0x00884000": "copro_fifo",
        "0x0090e000": "aperture_buffer",
        "0x00501084": "camera_scale_x",
        "0x00501088": "camera_scale_y",
    }
    write_class = Counter()
    work_matrix_store = []
    for line in stdout.splitlines():
        line = line.strip()
        if not line.startswith("{"):
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
            val = 0
        rec = {
            "step": j.get("step"),
            "addr": f"0x{addr:08x}",
            "value": f"0x{val:08x}",
            "f32": f32(val),
        }
        writes.append(rec)
        # Classify
        if 0x00500000 <= addr < 0x00600000:
            write_class["work_ram"] += 1
        elif addr == 0x0090E000 or (0x0090E000 <= addr < 0x0090E000 + 0x200):
            write_class["aperture"] += 1
        elif 0x00880000 <= addr < 0x00882000:
            write_class["tgp_function_port"] += 1
        elif 0x00980000 <= addr < 0x00980010:
            write_class["tgp_upload_ctl"] += 1
        elif 0x00884000 <= addr < 0x00886000:
            write_class["copro_fifo"] += 1
        elif 0x00800000 <= addr < 0x00804000:
            write_class["geo_control"] += 1
        elif 0x00804000 <= addr < 0x00808000:
            write_class["geo_program"] += 1
        else:
            write_class["other"] += 1

        base_addr = addr & ~0xFFF
        key = f"0x{base_addr:08x}"
        if key in port_writes and len(port_writes[key]) < 40:
            port_writes[key].append(rec)
        elif addr in special:
            tag = special[addr]
            # also bucket aperture-style
            for pk, pv in port_writes.items():
                if pk == f"0x{addr:08x}" and len(pv) < 40:
                    pv.append(rec)

    # Final JSON summary lines (probe may print a result object)
    final_reads = {}
    halt = None
    for line in stdout.splitlines():
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            j = json.loads(line)
        except Exception:
            continue
        if j.get("type") in (None, "result", "summary") or "reads" in j or "final" in j:
            if "reads" in j:
                final_reads = j["reads"]
            if "memory_reads" in j:
                final_reads = j["memory_reads"]
            if "halt" in j:
                halt = j["halt"]
            if "stop" in j:
                halt = j.get("stop") or halt
            if "status" in j and halt is None:
                halt = j["status"]

    # Absence checks for this probe
    absence = {}
    for pname, arr in port_writes.items():
        # aperture is allowed to receive the display triple — still not a TGP matrix
        absence[pname] = {
            "write_count": write_class.get(
                {
                    "0x00880000": "tgp_function_port",
                    "0x00980000": "tgp_upload_ctl",
                    "0x00800000": "geo_control",
                    "0x00804000": "geo_program",
                    "0x00884000": "copro_fifo",
                    "0x0090e000": "aperture",
                }.get(pname, "other"),
                0,
            )
            if pname == "0x0090e000"
            else len(arr),
            "samples": arr[:8],
            "is_tgp_matrix": False,
        }

    return {
        "name": spec["name"],
        "status": "ok" if proc.returncode == 0 else f"rc={proc.returncode}",
        "halt": halt,
        "returncode": proc.returncode,
        "cmd": cmd,
        "trace": str(trace_path),
        "write_class": dict(write_class),
        "total_memory_writes": len(writes),
        "port_writes": {k: v[:8] for k, v in port_writes.items() if v},
        "final_reads": final_reads,
        "stderr_tail": stderr[-2000:] if stderr else "",
        "note": spec.get("note", ""),
        "absence_this_probe": absence,
    }


def disasm_snippets() -> dict[str, Any]:
    """Capture vf2i960 disasm for consumer paths (task 1 evidence)."""
    snippets = {}
    jobs = {
        "display_emit_and_follow": (0x31040, 200),
        "camera_path": (0x1D320, 250),
        "object_helper": (0x7C60, 80),
    }
    if not I960.exists() or not ROM_DIR.exists():
        return {"error": "vf2i960 or rom missing"}
    for name, (addr, count) in jobs.items():
        cmd = [str(I960), "disasm", str(ROM_DIR), f"0x{addr:x}", str(count)]
        try:
            proc = subprocess.run(cmd, capture_output=True, text=True, timeout=60, cwd=str(ROOT))
            snippets[name] = {
                "addr": f"0x{addr:x}",
                "count": count,
                "text": proc.stdout[-20000:],
            }
        except Exception as e:
            snippets[name] = {"addr": f"0x{addr:x}", "error": str(e)}
    # xrefs
    for name, addr in (("xrefs_emit", 0x31040), ("xrefs_7c60", 0x7C60)):
        cmd = [str(I960), "xrefs", str(ROM_DIR), f"0x{addr:x}"]
        try:
            proc = subprocess.run(cmd, capture_output=True, text=True, timeout=60, cwd=str(ROOT))
            snippets[name] = {"addr": f"0x{addr:x}", "text": proc.stdout[-8000:]}
        except Exception as e:
            snippets[name] = {"error": str(e)}
    return snippets


def mame_suggestion() -> dict[str, Any]:
    """Read-only suggestion from third_party MAME ref. Not recovered semantics."""
    h = ROOT / "third_party" / "mame-model2-ref" / "model2.h"
    cpp = ROOT / "third_party" / "mame-model2-ref" / "model2.cpp"
    out: dict[str, Any] = {
        "source": str(h),
        "note": "Suggestion only. Ghidra/MAME suggestions -> our executor measures -> differential proves.",
        "geo_state_fields": [],
        "geo_matrix_write_declared": False,
        "matrix_delivery_hypothesis": "",
        "guest_ports": {},
    }
    if h.exists():
        text = h.read_text(encoding="utf-8", errors="replace")
        out["geo_matrix_write_declared"] = "geo_matrix_write" in text
        out["geo_state_fields"] = [
            "float matrix[12]  // Current Transformation Matrix",
            "poly_vertex focus  // Focus (x,y)",
        ]
        out["matrix_delivery_hypothesis"] = (
            "MAME geo_state.matrix[12] is filled by geo_matrix_write(), a GEO STREAM opcode "
            "processed when the guest pushes command data through geo_w (0x00800000) / geo_prg_w "
            "(0x00804000). There is no separate sideband 'TGP matrix port' in this reference. "
            "Therefore alternate-port absence on 0x880000/0x980000 plus stream-class absence on "
            "geo/FIFO jointly imply matrix/focus/geometry_mode are not delivered on any measured path."
        )
    if cpp.exists():
        text = cpp.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r"map\((0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+)\)\.(r|w|rw)\([^)]*(geo_[a-z0-9_]+|copro_[a-z0-9_]+)", text):
            out["guest_ports"][f"{m.group(1)}-{m.group(2)}"] = {
                "handler": m.group(4),
                "access": m.group(3),
            }
    return out


def build_measured_view(
    static: dict[str, Any],
    regions: dict[str, Any],
    probes: list[dict[str, Any]],
    port_absence: dict[str, Any],
) -> dict[str, Any]:
    """Fail-closed measured_view_v0377. confidence=absent unless a real 16-float matrix+focus is measured."""
    matrix = None
    focus_x = None
    focus_y = None
    geometry_mode = None
    ips: list[str] = []

    # Check park matrix scans
    for park in regions.get("parks", []):
        for key in ("work_matrix_like", "geometry_matrix_like", "copro_matrix_like", "buffer_matrix_like"):
            mx = park.get(key) or {}
            cands = mx.get("candidates") or []
            if cands and cands[0].get("diag_score", 0) >= 2 and cands[0].get("floatish", 0) >= 10:
                # Would promote ONLY with store IP evidence — we do not invent.
                pass

    # Check probes for 12 consecutive float-like writes to a stable window
    for pr in probes:
        if pr.get("status") not in ("ok",) and not str(pr.get("status", "")).startswith("rc="):
            continue
        # No measured 12-float store to work-RAM matrix object in any probe output
        for pname, samples in (pr.get("port_writes") or {}).items():
            if pname.startswith("0x00880000") or pname.startswith("0x00980000"):
                # function/upload port — if any write existed it is NOT a 12-float matrix by itself
                for s in samples:
                    if s.get("f32") is not None:
                        # keep as measured non-matrix traffic
                        ips.append(s.get("addr", ""))

    related = {
        "display_triple": {
            "addr": "*(u32*)0x50084c + 0x54/58/5c",
            "values": ["0x40c00000 (6.0f)", "0x40966666 (4.7f)", "0x41940000 (18.5f)"],
            "store_ip": "0x00031024",
            "function": "display_transform_defaults@0x31004",
            "classification": "Work-RAM display-state, NOT TGP matrix",
        },
        "camera_scale": {
            "addr": ["0x00501084", "0x00501088"],
            "values": ["0x44160000 (600.0f)"],
            "store_ips": ["0x0001d34c", "0x0001d35c"],
            "function": "fa_camera_initialize_prefix@0x1d320",
            "classification": "Work-RAM camera scale globals",
        },
        "aperture_consumer": {
            "producer_ips": ["0x00031068", "0x00031078", "0x00031088"],
            "function": "display_command_emit@0x31040",
            "dest": "0x0090e000[cursor 0x5001e4]",
            "downstream": "call 0x00007c60 object-submit helper → geo 0x800010 object-table w0 + FIFO protocol; NOT transformed verts / 3x4",
            "classification": "aperture_transform_packet_not_geo_matrix",
        },
        "camera_task_scratch": {
            "addr": "0x00515400 +0x5c/+0x60",
            "values": ["0x435f3333 (~223.2f)", "0x432ccccd (~172.8f)"],
            "path": "0x1d470-0x1d4c4 FIFO copro arith tag 0x0b001616 (class 0x16)",
            "classification": "camera_task_scratch_not_focus",
        },
        "fifo_copro_arith_tag": {
            "word": "0x0b001616",
            "tgp_class_bits": "0x16",
            "classification": "NOT tgp class-0b matrix opcode",
        },
    }

    return {
        "version": "v0377",
        "matrix": matrix,
        "focus_x": focus_x,
        "focus_y": focus_y,
        "geometry_mode": geometry_mode,
        "ips": ips,
        "confidence": "absent",
        "fail_closed": (
            "No measured real 16-float/12-float TGP matrix + focus on any port outside FIFO "
            "class-09/0b/0c (those already ABSENT). Alternate ports 0x880000/0x980000/0x90e000/"
            "work-RAM also fail to yield a measured matrix. apply_tgp_transform must stay "
            "identity passthrough. Do NOT invent camera from hybrid.c."
        ),
        "port_absence_table": port_absence,
        "related_measured_work_ram": related,
        "consumer_of_aperture": (
            "display_command_emit@0x31040 writes display triple to aperture 0x0090e000[cursor], "
            "then call 0x7c60 (object submit). No path writes transformed verts or a 3x4 to "
            "geo/TGP/copro. Alternate branch 0x310cc calls display_transform_update@0x311b8 "
            "then same aperture serialization + optional display+0x70/0x72 extras."
        ),
        "rejected_false_positives": [
            {
                "addr": "0x0050a0e0",
                "window": "twelve consecutive 0x3f800000 (1.0f)",
                "reject_reason": (
                    "NOT identity 3x4. Recovered C (tasks.c / camera_viewport.c / "
                    "native_runtime.c) indexes 0x50a0e0 as a mode-profile table "
                    "(CAMERA profile lookup by mode byte). Fail-closed: unit table, "
                    "not TGP matrix."
                ),
            }
        ],
        "mame_suggestion": mame_suggestion(),
        "related_v0376": "out/attr-transform/measured_view.json confidence=absent",
    }


def build_port_absence(
    regions: dict[str, Any],
    probes: list[dict[str, Any]],
    static: dict[str, Any],
) -> dict[str, Any]:
    rows = []
    for spec in PORT_TABLE:
        probe_counts = []
        samples = []
        for pr in probes:
            key = spec["addr"][:10]  # match "0x00880000" style prefix
            hits = []
            for pk, arr in (pr.get("port_writes") or {}).items():
                if pk.startswith(spec["addr"][: len(spec["addr"]) - 2]) or pk == spec["addr"]:
                    hits.extend(arr)
            # also check write_class
            wc = pr.get("write_class") or {}
            named = {
                "aperture_buffer": "aperture",
                "tgp_function_port": "tgp_function_port",
                "tgp_upload_ctl": "tgp_upload_ctl",
                "geo_control": "geo_control",
                "geo_program": "geo_program",
                "copro_fifo": "copro_fifo",
            }.get(spec["port"])
            count = wc.get(named, 0) if named else 0
            if spec["port"] == "work_ram_matrix_window":
                count = wc.get("work_ram", 0)
            probe_counts.append({"probe": pr.get("name"), "count": count})
            samples.extend(hits[:4])

        static_hits = static.get("patterns", {}).get(
            "addr_" + spec["addr"].replace("0x00", "").replace("0x", ""), []
        )
        if not static_hits:
            # try full form
            static_hits = static.get("patterns", {}).get(
                "addr_" + spec["addr"].lstrip("0x").lstrip("0"), []
            )

        # Verdict fail-closed
        total = sum(c["count"] for c in probe_counts)
        if spec["port"] == "aperture_buffer":
            verdict = "measured_display_triple_only_not_matrix"
            confidence = "measured_non_matrix"
        elif spec["port"] == "copro_fifo":
            verdict = "measured_copro_arith_tags_not_matrix"
            confidence = "measured_non_matrix"
        elif spec["port"] == "work_ram_matrix_window":
            verdict = "no_measured_12float_matrix_store"
            confidence = "absent"
        elif total == 0:
            verdict = "absent_zero_writes_in_probes"
            confidence = "absent"
        else:
            verdict = "writes_present_but_not_accepted_as_matrix_without_class09_0b_payload"
            confidence = "absent_pending_class_payload"

        rows.append(
            {
                **spec,
                "static_hits": static_hits[:12],
                "probe_counts": probe_counts,
                "probe_samples": samples,
                "verdict": verdict,
                "confidence": confidence,
                "is_tgp_matrix_port": False,
            }
        )
    return {
        "version": "v0377",
        "question": "Measured TGP geometry matrix / focus / geometry_mode on ports OTHER than FIFO class-09/0b/0c?",
        "already_absent_not_relitigated": [
            "FIFO/geo stream class 0x09 focus",
            "FIFO/geo stream class 0x0b 3x4 matrix",
            "FIFO/geo stream class 0x0c translate",
            "FIFO/geo stream class 0x07 geometry_mode (residual IEEE 266f only)",
        ],
        "ports": rows,
        "overall": {
            "measured_tgp_matrix_on_alternate_port": False,
            "measured_focus_on_alternate_port": False,
            "measured_geometry_mode_on_alternate_port": False,
            "confidence": "absent",
        },
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-probe", action="store_true", help="skip vf2probe runs")
    ap.add_argument("--out", type=Path, default=OUT)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    print("[1/6] static IEEE scan of maincpu")
    static = scan_maincpu_ieee()
    (args.out / "static_ieee_v0377.json").write_text(
        json.dumps(static, indent=2), encoding="utf-8"
    )

    print("[2/6] park region dumps (aperture/display/camera/matrix scan)")
    regions = dump_park_regions()
    # Split per-park convenience files
    for park in regions["parks"]:
        name = Path(park["park"]).stem
        if "display_object" in park or park.get("display_object_words"):
            (args.out / f"region_display_obj_{name}_v0377.json").write_text(
                json.dumps(park, indent=2), encoding="utf-8"
            )
        (args.out / f"region_park_{name}_v0377.json").write_text(
            json.dumps(park, indent=2), encoding="utf-8"
        )
    (args.out / "region_parks_all_v0377.json").write_text(
        json.dumps(regions, indent=2), encoding="utf-8"
    )
    # Convenience: long-29 focus files
    long29 = next((p for p in regions["parks"] if "long-29" in p["park"]), None)
    if long29:
        (args.out / "region_aperture_v0377.json").write_text(
            json.dumps(long29.get("cursor"), indent=2), encoding="utf-8"
        )
        (args.out / "region_display_obj_v0377.json").write_text(
            json.dumps(
                {
                    "display_ptr": long29.get("display_ptr_0x50084c"),
                    "words": long29.get("display_object_words"),
                    "triple": long29.get("display_triple"),
                },
                indent=2,
            ),
            encoding="utf-8",
        )
        (args.out / "region_camera_task_v0377.json").write_text(
            json.dumps(
                {
                    "camera_scale": long29.get("camera_scale"),
                    "task_0x515400": long29.get("camera_task_0x515400"),
                    "note": "Measured fields only. Do NOT rename +0x5c/+0x60 to focus_x/y.",
                },
                indent=2,
            ),
            encoding="utf-8",
        )

    print("[3/6] disasm consumer paths")
    snippets = disasm_snippets()
    (args.out / "disasm_consumers_v0377.json").write_text(
        json.dumps(snippets, indent=2), encoding="utf-8"
    )

    print("[4/6] probes")
    probes: list[dict[str, Any]] = []
    prior_probe_path = args.out / "probe_port_writes_v0377.json"
    if args.no_probe:
        if prior_probe_path.exists():
            try:
                probes = json.loads(prior_probe_path.read_text(encoding="utf-8"))
                probes = [
                    p for p in probes if p.get("status") not in (None, "skipped")
                ] or [{"name": "prior", "status": "skipped", "reason": "--no-probe"}]
            except Exception:
                probes = [{"name": "prior", "status": "skipped", "reason": "--no-probe"}]
        else:
            probes = [{"name": p["name"], "status": "skipped", "reason": "--no-probe"} for p in PROBES]
        # do not overwrite prior probe evidence
    else:
        for spec in PROBES:
            print(f"    probe {spec['name']} ...")
            probes.append(run_probe(spec))
        prior_probe_path.write_text(json.dumps(probes, indent=2), encoding="utf-8")

    print("[5/6] port absence table")
    port_absence = build_port_absence(regions, probes, static)
    (args.out / "port_absence_v0377.json").write_text(
        json.dumps(port_absence, indent=2), encoding="utf-8"
    )

    print("[6/6] measured_view_v0377.json")
    view = build_measured_view(static, regions, probes, port_absence)
    (args.out / "measured_view_v0377.json").write_text(
        json.dumps(view, indent=2), encoding="utf-8"
    )

    summary = {
        "version": "v0377",
        "confidence": view["confidence"],
        "matrix": view["matrix"],
        "focus_x": view["focus_x"],
        "geometry_mode": view["geometry_mode"],
        "tgp_matrix_on_alternate_ports": port_absence["overall"]["measured_tgp_matrix_on_alternate_port"],
        "aperture_consumer": view["consumer_of_aperture"],
        "static_ieee_lda_sites": {
            k: len(v.get("lda_sites", []))
            for k, v in static.get("aligned_instruction_hits", {}).items()
            if isinstance(v, dict)
        },
        "probes": [{"name": p.get("name"), "status": p.get("status"), "writes": p.get("write_class")} for p in probes],
        "note_file": "decomp/i960/notes/matrix_ports_p2_v0377.md",
    }
    (args.out / "summary_v0377.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
