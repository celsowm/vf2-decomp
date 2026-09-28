#!/usr/bin/env python3
"""Explore guest i960 edges that write geometry/FIFO ports (v0378).

Coverage unit is guest edge ip_before -> ip_after, not host C coverage.
Fail-closed: only measured probe/memory-trace events are reported.

Subcommands:
  static   rank ROM functions by proximity to geometry submit (0x7c60/0x7f24/geo)
  mine     extract baseline geo/FIFO-write edges from existing JSONL traces
  run      controlled probe mutations from parks; record edges that write geo/FIFO
  aggregate  combine static + mine + run into out/attr-geo-edges/
  snippet  capture one minimized memory-trace window for a candidate edge
"""
from __future__ import annotations

import argparse
import json
import struct
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROBE = ROOT / "build/Debug/vf2probe.exe"
I960 = ROOT / "build/Debug/vf2i960.exe"
ROM_DIR = ROOT / "roms/vf2"
OUT = ROOT / "out/attr-geo-edges"
FUNCTIONS_CSV = ROOT / "decomp/i960/functions.csv"

HELPER_SUBMIT = 0x00007C60
HELPER_PALETTE = 0x00007F24
POL_TEST_TASK = 0x00021A00
TABLE = 0x020E0004

# Measured Model 2A geo/FIFO windows (notes v0364–v0377)
GEO_CTRL = 0x00800000
GEO_PROG = 0x00804000
GEO_HI = 0x00808000
FIFO_BASE = 0x00880000
FIFO_DATA = 0x00884000
FIFO_HI = 0x00888000
PORT_MATRIX_ABSENT = (0x00880000, 0x00980000, 0x00800000)

# Known measured submit IPs (v0372–v0377 helper/palette/pol_test/emit/camera)
BASELINE_SUBMIT_IPS = {
    0x00007C74, 0x00007C7C, 0x00007C88, 0x00007C9C, 0x00007CA4,
    0x00007CB8, 0x00007CC4, 0x00007CD4, 0x00007CF0, 0x00007D04,
    0x00007D08, 0x00007D0C, 0x00007D10,
    0x00007F24, 0x00007F28, 0x00007F30, 0x00007F34, 0x00007F38,
    0x00007F3C, 0x00007F40, 0x00007F44, 0x00007F48, 0x00007F4C,
    0x00007F50, 0x00007F54, 0x00007F58, 0x00007F5C, 0x00007F60,
    # pol_test task 0x21a00 family (packet_format_p1_v0377)
    0x00021A40, 0x00021A44, 0x00021A48, 0x00021A4C, 0x00021A50,
    0x00021A54, 0x00021A58, 0x00021A5C, 0x00021A60, 0x00021A64,
    0x00021A68, 0x00021A6C, 0x00021A70, 0x00021A74, 0x00021A78,
    0x00021A7C, 0x00021A80, 0x00021A84, 0x00021A88, 0x00021A8C,
    0x00021A90, 0x00021A94, 0x00021A98, 0x00021A9C, 0x00021AA0,
    0x00021AA4, 0x00021AA8, 0x00021AAC, 0x00021AB0, 0x00021AB4,
    0x00021AB8, 0x00021ABC, 0x00021AC0, 0x00021AC4, 0x00021AC8,
    0x00021ACC, 0x00021AD0, 0x00021AD4, 0x00021AD8, 0x00021ADC,
    0x00021AE0, 0x00021AE4, 0x00021AE8, 0x00021AEC, 0x00021AF0,
    0x00021AF4, 0x00021AF8, 0x00021AFC, 0x00021B00,
    0x00021B04, 0x00021B08, 0x00021B0C, 0x00021B10, 0x00021B14,
    0x00021B18, 0x00021B1C, 0x00021B20, 0x00021B24, 0x00021B28,
    0x00021B2C, 0x00021B30, 0x00021B34, 0x00021B38, 0x00021B3C,
    0x00021B40, 0x00021B44, 0x00021B48, 0x00021B4C, 0x00021B50,
    0x00021B54, 0x00021B58, 0x00021B5C, 0x00021B60, 0x00021B64,
    0x00021B68, 0x00021B6C, 0x00021B70, 0x00021B74, 0x00021B78,
    0x00021B7C, 0x00021B80, 0x00021B84, 0x00021B88, 0x00021B8C,
    0x00021B90, 0x00021B94, 0x00021B98, 0x00021B9C, 0x00021BA0,
    0x00021BA4, 0x00021BA8, 0x00021BAC, 0x00021BB0, 0x00021BB4,
    0x00021BB8, 0x00021BBC, 0x00021BC0, 0x00021BC4, 0x00021BC8,
    0x00021BCC, 0x00021BD0, 0x00021BD4, 0x00021BD8, 0x00021BDC,
    0x00021BE0, 0x00021BE4, 0x00021BE8,
    # display_command_emit / camera FIFO arith (v0376/v0377)
    0x00031040, 0x00031044, 0x00031048, 0x0003104C, 0x00031050,
    0x00031054, 0x00031058, 0x0003105C, 0x00031060, 0x00031064,
    0x00031068, 0x0003106C, 0x00031070, 0x00031074, 0x00031078,
    0x0003107C, 0x00031080, 0x00031084, 0x00031088, 0x0003108C,
    0x00031090, 0x00031094, 0x00031098, 0x0003109C, 0x000310A0,
    0x000310A4, 0x000310A8, 0x000310AC, 0x000310B0, 0x000310B4,
    0x000310B8, 0x000310BC, 0x000310C0, 0x000310C4, 0x000310C8,
    0x000311B8,
    0x0001D320, 0x0001D34C, 0x0001D35C, 0x0001D458, 0x0001D470,
    0x0001D478, 0x0001D48C, 0x0001D498, 0x0001D4B8, 0x0001D4C4,
    0x0001D4C8, 0x0001D4CC, 0x0001D4D4, 0x0001D4D8, 0x0001D4E4,
    0x0001D514, 0x0001D544, 0x0001D5C4, 0x0001D5DC, 0x0001D5F4,
    0x0001D610, 0x0001D634, 0x0001D640, 0x0001D660,
}

# Measured FIFO protocol words (not geo-stream payloads)
PROTOCOL_WORDS = {
    0x00800101, 0x01800303, 0x03000606, 0x1A003434, 0x1000202,
    0x3E9EB852, 0x3E428F5C, 0x3F9EB852, 0x37806F6F, 0x0B001616,
    0x12002424, 0x13802727,
}


def load_recovered_ranges() -> list[tuple[int, int, str, str]]:
    ranges: list[tuple[int, int, str, str]] = []
    if not FUNCTIONS_CSV.exists():
        return ranges
    for ln in FUNCTIONS_CSV.read_text(encoding="utf-8", errors="replace").splitlines()[1:]:
        parts = ln.split(",")
        if len(parts) < 4:
            continue
        try:
            a = int(parts[0], 16)
            b = int(parts[1], 16)
        except ValueError:
            continue
        ranges.append((a, b, parts[2], parts[3]))
    return ranges


def recovered_at(ip: int | None, ranges: list[tuple[int, int, str, str]]) -> dict | None:
    if ip is None:
        return None
    for a, b, name, status in ranges:
        if a <= ip < b:
            return {"start": f"0x{a:08x}", "end": f"0x{b:08x}", "name": name, "status": status}
    return None


def is_geo_or_fifo(addr: int) -> bool:
    if addr == 0x00800010 or GEO_PROG <= addr < GEO_HI:
        return True
    if GEO_CTRL <= addr < GEO_HI:
        return True
    if FIFO_BASE <= addr < FIFO_HI:
        return True
    return False


def is_known_submit_ip(ip: int | None) -> bool:
    if ip is None:
        return False
    if ip in BASELINE_SUBMIT_IPS:
        return True
    if HELPER_SUBMIT <= ip <= HELPER_PALETTE + 0x50:
        return True
    if 0x00021A00 <= ip <= 0x00021C00:
        return True
    return False


def build_maincpu() -> bytes:
    region = bytearray(0x200000)
    for name, off in (
        ("epr-18385.12", 0),
        ("epr-18386.13", 2),
        ("epr-18387.14", 0x40000),
        ("epr-18388.15", 0x40002),
    ):
        src = (ROM_DIR / name).read_bytes()
        for i in range(0, len(src), 2):
            di = off + i * 2
            if di + 1 < len(region):
                region[di] = src[i]
                region[di + 1] = src[i + 1]
    return bytes(region)


def call_targets(img: bytes, target: int) -> list[int]:
    """i960 COBR call/branch: target = ip + sign_extend(imm24) (MEASURED).

    Verified at 0x21a9c word 0x09fe61c4 -> 0x21a9c + (-0x19e3c) = 0x7c60.
    """
    hits = []
    for ip in range(0, len(img) - 4, 4):
        word = struct.unpack_from("<I", img, ip)[0]
        opcode = word >> 24
        # 0x08 = b, 0x09 = call (disasm-measured on maincpu.bin)
        if opcode not in (0x08, 0x09):
            continue
        disp = word & 0xFFFFFF
        if disp & 0x800000:
            disp -= 0x1000000
        if ((ip + disp) & 0xFFFFFFFF) == target:
            hits.append(ip)
    return hits


def scan_imm_addrs(img: bytes, imm: int) -> list[int]:
    """Find 32-bit little-endian immediates equal to imm (call/lda/st forms)."""
    pat = struct.pack("<I", imm)
    hits = []
    start = 0
    while True:
        i = img.find(pat, start)
        if i < 0:
            break
        if i % 4 == 0:
            hits.append(i)
        start = i + 1
    return hits


def scan_w2_like(img: bytes, limit: int = 40) -> list[int]:
    """ROM words with bit-23 set and low 23 bits nonzero (poly-ROM w2 pattern)."""
    hits = []
    for ip in range(0, len(img) - 4, 4):
        w = struct.unpack_from("<I", img, ip)[0]
        if w & 0x00800000 and (w & 0x007FFFFF) and (w >> 28) in (0x00, 0x01):
            hits.append(ip)
            if len(hits) >= limit:
                break
    return hits


def static_rank() -> dict:
    img = build_maincpu()
    ranges = load_recovered_ranges()
    calls_7c60 = call_targets(img, HELPER_SUBMIT)
    calls_7f24 = call_targets(img, HELPER_PALETTE)
    imm_geo = scan_imm_addrs(img, 0x00800010) + scan_imm_addrs(img, 0x00804000)
    imm_geo += scan_imm_addrs(img, 0x00800000)
    w2_sites = scan_w2_like(img)

    # Cluster call sites into function windows via functions.csv when possible
    def cluster(sites: list[int]) -> list[dict]:
        out = []
        for ip in sites:
            rec = recovered_at(ip, ranges)
            out.append({
                "ip": f"0x{ip:08x}",
                "recovered": rec,
                "proximity_submit": abs(ip - HELPER_SUBMIT),
                "proximity_palette": abs(ip - HELPER_PALETTE),
                "in_pol_test_cluster": 0x00020000 <= ip < 0x00022000,
                "in_emit_cluster": 0x00030000 <= ip < 0x00032000,
                "in_camera_cluster": 0x0001D000 <= ip < 0x0001E000,
            })
        out.sort(key=lambda r: r["proximity_submit"])
        return out

    # Rank: call sites of 0x7c60 first (object submit), then 0x7f24, then geo imm
    ranked = []
    for ip in calls_7c60:
        rec = recovered_at(ip, ranges)
        ranked.append({
            "kind": "call_7c60",
            "ip": f"0x{ip:08x}",
            "recovered": rec,
            "score": 1000 - min(999, abs(ip - HELPER_SUBMIT) // 16),
            "geometry_submit_proximity": "direct_call_object_submit",
        })
    for ip in calls_7f24:
        rec = recovered_at(ip, ranges)
        ranked.append({
            "kind": "call_7f24",
            "ip": f"0x{ip:08x}",
            "recovered": rec,
            "score": 800 - min(799, abs(ip - HELPER_PALETTE) // 16),
            "geometry_submit_proximity": "direct_call_palette_geo_ram",
        })
    for ip in sorted(set(imm_geo)):
        rec = recovered_at(ip, ranges)
        ranked.append({
            "kind": "imm_geo_port",
            "ip": f"0x{ip:08x}",
            "recovered": rec,
            "score": 400,
            "geometry_submit_proximity": "rom_immediate_geo_port",
        })
    ranked.sort(key=lambda r: -r["score"])

    report = {
        "helpers": {
            "object_submit": f"0x{HELPER_SUBMIT:08x}",
            "palette": f"0x{HELPER_PALETTE:08x}",
            "pol_test_task": f"0x{POL_TEST_TASK:08x}",
        },
        "counts": {
            "call_7c60": len(calls_7c60),
            "call_7f24": len(calls_7f24),
            "imm_geo_port": len(set(imm_geo)),
            "w2_like_rom_words_sampled": len(w2_sites),
        },
        "call_7c60_sites": cluster(calls_7c60),
        "call_7f24_sites": cluster(calls_7f24),
        "imm_geo_sites": [
            {"ip": f"0x{ip:08x}", "recovered": recovered_at(ip, ranges)}
            for ip in sorted(set(imm_geo))[:40]
        ],
        "w2_like_sample": [
            {
                "ip": f"0x{ip:08x}",
                "word": f"0x{struct.unpack_from('<I', img, ip)[0]:08x}",
                "w2_index": f"0x{struct.unpack_from('<I', img, ip)[0] & 0x7FFFFF:07x}",
            }
            for ip in w2_sites[:20]
        ],
        "ranked_top": ranked[:40],
        "note": (
            "Static proximity only. Guest geometry submit is measured at helper "
            "0x7c60 (object-table w0/w1/w2 + FIFO protocol) and 0x7f24 "
            "(6 words to (g10)[g12]). Poly vertices are consumed by TGP via w2 "
            "OUTSIDE the guest oracle (v0377 P2 sideband-port negative stands)."
        ),
    }
    return report


def mine_trace(path: Path, ranges: list[tuple[int, int, str, str]]) -> dict:
    """Extract guest edges whose correlated instruction wrote geo/FIFO."""
    step_ip: dict[int, tuple[int | None, int | None]] = {}
    pending: list[dict] = []
    edge_writes: dict[tuple[int | None, int | None], list[dict]] = defaultdict(list)
    call_edges: Counter = Counter()
    fifo_values: Counter = Counter()
    geo_values: Counter = Counter()
    n_mem = 0
    n_geo_fifo_w = 0

    def flush() -> None:
        nonlocal pending, n_geo_fifo_w
        for mem in pending:
            if mem.get("kind") != "write":
                continue
            if mem.get("type") != "memory":
                continue
            step = mem.get("step")
            ip_b, ip_a = step_ip.get(step, (None, None))
            addr = int(mem.get("address", 0))
            if not is_geo_or_fifo(addr):
                continue
            try:
                val = int.from_bytes(bytes.fromhex(mem.get("bytes") or "00")[:4], "little")
            except Exception:
                val = 0
            n_geo_fifo_w += 1
            if GEO_CTRL <= addr < GEO_HI:
                geo_values[val] += 1
            else:
                fifo_values[val] += 1
            key = (ip_b, ip_a)
            if len(edge_writes[key]) < 8:
                edge_writes[key].append({
                    "address": f"0x{addr:08x}",
                    "value": f"0x{val:08x}",
                    "step": step,
                    "protocol_like": val in PROTOCOL_WORDS or ((val >> 24) & 0x3F) in (
                        0x00, 0x01, 0x02, 0x07, 0x09, 0x0B, 0x0C
                    ) and (val & 0xFF00FF00) == 0x00000000 and val != 0,
                })
            else:
                edge_writes[key].append({
                    "address": f"0x{addr:08x}",
                    "value": f"0x{val:08x}",
                    "step": step,
                })
        pending = []

    with path.open("r", errors="replace") as f:
        for ln in f:
            if not ln.startswith("{"):
                continue
            try:
                j = json.loads(ln)
            except Exception:
                continue
            t = j.get("type")
            if t == "memory":
                n_mem += 1
                pending.append(j)
            elif t == "step":
                ip_b = j.get("ip_before")
                ip_a = j.get("ip_after")
                step_ip[j.get("step")] = (ip_b, ip_a)
                if pending:
                    flush()
                if ip_a == HELPER_SUBMIT:
                    call_edges[ip_b] += 1
                if ip_a == HELPER_PALETTE:
                    call_edges[ip_b] += 1
        flush()

    edges = []
    for (ip_b, ip_a), writes in edge_writes.items():
        # Multi-word geo-stream push: >1 write at geo/FIFO from same edge
        # beyond table w0 single-store pattern at 0x7d08.
        addrs = [w["address"] for w in writes]
        n_geo = sum(1 for a in addrs if a.startswith("0x0080"))
        n_fifo = sum(1 for a in addrs if a.startswith("0x0088"))
        multi = len(writes) >= 3 and n_geo >= 2
        values = [w.get("value") for w in writes]
        edges.append({
            "ip_before": f"0x{ip_b:08x}" if ip_b is not None else None,
            "ip_after": f"0x{ip_a:08x}" if ip_a is not None else None,
            "witness_count": len(writes),
            "write_addrs": sorted(set(addrs)),
            "sample_values": values[:6],
            "n_geo_writes": n_geo,
            "n_fifo_writes": n_fifo,
            "known_submit_site": is_known_submit_ip(ip_b),
            "recovered_before": recovered_at(ip_b, ranges),
            "recovered_after": recovered_at(ip_a, ranges),
            "looks_like_geo_stream_push": bool(multi and not is_known_submit_ip(ip_b)),
            "writes": writes[:6],
        })
    edges.sort(key=lambda e: (-e["witness_count"], e["ip_before"] or ""))
    return {
        "path": str(path),
        "n_memory": n_mem,
        "n_geo_fifo_writes": n_geo_fifo_w,
        "n_edges": len(edges),
        "call_edges_to_helpers": {
            (f"0x{k:08x}" if k is not None else None): v
            for k, v in call_edges.most_common(30)
        },
        "top_geo_values": [(f"0x{v:08x}", n) for v, n in geo_values.most_common(16)],
        "top_fifo_values": [(f"0x{v:08x}", n) for v, n in fifo_values.most_common(16)],
        "edges": edges,
    }


def run_probe(
    name: str,
    snapshot: Path,
    sets: list[str],
    set_ip: str | None,
    until: str | None,
    max_steps: int,
    set_regs: list[str] | None = None,
    memory_trace: bool = True,
) -> Path:
    OUT.mkdir(parents=True, exist_ok=True)
    jsonl = OUT / f"probe_{name}.jsonl"
    cmd = [
        str(PROBE),
        "--rom-dir", str(ROM_DIR),
        "--snapshot", str(snapshot),
        "--max-steps", str(max_steps),
        "--output-snapshot", str(OUT / f"probe_{name}.vf2snap"),
    ]
    if memory_trace:
        cmd.append("--memory-trace")
        cmd.append("--trace")
    if set_ip:
        cmd += ["--set-ip", set_ip]
    if until:
        cmd += ["--until", until]
    for s in sets:
        if s.startswith("u8:"):
            cmd += ["--set-u8", s[3:]]
        elif s.startswith("u16:"):
            cmd += ["--set-u16", s[4:]]
        else:
            cmd += ["--set-u32", s]
    for r in set_regs or []:
        cmd += ["--set-reg", r]
    p = subprocess.run(cmd, capture_output=True, text=True, cwd=str(ROOT))
    text = (p.stdout or "") + "\n" + (p.stderr or "")
    lines = [ln for ln in text.splitlines() if ln.startswith("{")]
    jsonl.write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")
    meta = OUT / f"probe_{name}.meta.json"
    halt = None
    for ln in reversed(text.splitlines()):
        if '"type":"final"' in ln:
            try:
                halt = json.loads(ln)
            except Exception:
                halt = {"raw": ln[-300:]}
            break
    meta.write_text(json.dumps({
        "name": name,
        "snapshot": str(snapshot),
        "cmd": cmd,
        "returncode": p.returncode,
        "n_json_lines": len(lines),
        "halt": halt,
        "stderr_tail": (p.stderr or "")[-400:],
    }, indent=2), encoding="utf-8")
    return jsonl


def controlled_cases() -> list[dict]:
    long29 = ROOT / "out/attr-long/long-29.vf2snap"
    long14 = ROOT / "out/attr-long/long-14.vf2snap"
    long31 = ROOT / "out/attr-long/long-31.vf2snap"
    all0 = ROOT / "out/attr-fs/all0-ready1.vf2snap"
    sixth = ROOT / "out/sixth-fresh.vf2snap"
    cases: list[dict] = []

    # pol_test gate open + path A/B mode sweep on long-29 and sixth
    for park_tag, park in (("l29", long29), ("sf", sixth), ("l14", long14)):
        if not park.exists():
            continue
        for mode in (0, 1, 2, 3):
            cases.append({
                "name": f"pol_{park_tag}_mode{mode}",
                "snapshot": park,
                "sets": [
                    "0x00501018=0x1000",
                    "0x0050101c=0x0",
                    f"0x00530150={mode}",
                    "0x0053014c=2",
                    "0x0053014d=2",
                    "0x0053014e=2",
                ],
                "set_ip": "0x00021a00",
                "until": "0x00021be8",
                "max_steps": 8000,
                "set_regs": None,
            })
        # helper 0x7c60 isolated with varied g0/g1
        for oid, g1 in ((0x985, 0), (0x97d, 0), (0x148, 0), (0x97d, 1), (0xEE1, 0)):
            cases.append({
                "name": f"h7c60_{park_tag}_{oid:03x}_g{g1}",
                "snapshot": park,
                "sets": [
                    "0x00501018=0x1000",
                    "0x0050101c=0x0",
                ],
                "set_ip": "0x00007c60",
                "until": "0x00007d14",
                "max_steps": 4000,
                "set_regs": [f"g0={oid:#x}", f"g1={g1:#x}"],
            })
        # helper 0x7f24 palette path
        cases.append({
            "name": f"h7f24_{park_tag}",
            "snapshot": park,
            "sets": [],
            "set_ip": "0x00007f24",
            "until": "0x00007f64",
            "max_steps": 2000,
            "set_regs": ["g0=0x501400", "g1=0"],
        })

    # Attract phase neighborhood: nav open + phase/counters + geometry gate resume
    if long29.exists():
        for tag, sets, ip, until in (
            ("phase5cd1", ["0x00500024=1", "0x00500704=0"], "0x0000a6c0", None),
            ("phase8ready0", ["0x00550000=0", "0x00500704=0"], "0x0000a6c0", None),
            ("phase10mask", ["0x00500028=0x00030001", "0x00500704=0", "0x00550000=0"], "0x0000a6c0", None),
            ("navgate", ["0x00500704=0", "0x00550000=0", "0x00500024=1"], "0x0000a748", None),
            ("geo_gate_resume", ["0x00500704=0", "0x00550000=0"], "0x0000a6c0", None),
        ):
            cases.append({
                "name": f"attr_l29_{tag}",
                "snapshot": long29,
                "sets": sets,
                "set_ip": ip,
                "until": until,
                "max_steps": 12000,
                "set_regs": None,
            })
    if all0.exists():
        cases.append({
            "name": "attr_all0_nav0",
            "snapshot": all0,
            "sets": ["0x00500704=0", "0x00550000=0", "0x00500024=1"],
            "set_ip": "0x0000a6c0",
            "until": None,
            "max_steps": 12000,
            "set_regs": None,
        })
    return cases


def run_all(filter_name: str | None = None) -> dict:
    ranges = load_recovered_ranges()
    cases = controlled_cases()
    ran = []
    for case in cases:
        if filter_name and filter_name not in case["name"]:
            continue
        if not case["snapshot"].exists():
            ran.append({"name": case["name"], "skipped": "missing_snapshot"})
            continue
        print(f"RUN {case['name']}", flush=True)
        jsonl = run_probe(
            case["name"],
            case["snapshot"],
            case["sets"],
            case["set_ip"],
            case["until"],
            case["max_steps"],
            case.get("set_regs"),
        )
        mined = mine_trace(jsonl, ranges)
        ran.append({
            "name": case["name"],
            "snapshot": str(case["snapshot"]),
            "sets": case["sets"],
            "set_ip": case["set_ip"],
            "until": case["until"],
            "max_steps": case["max_steps"],
            "set_regs": case.get("set_regs"),
            "jsonl": str(jsonl),
            "summary": {
                "n_edges": mined["n_edges"],
                "n_geo_fifo_writes": mined["n_geo_fifo_writes"],
                "call_edges_to_helpers": mined["call_edges_to_helpers"],
                "top_geo_values": mined["top_geo_values"][:8],
                "top_fifo_values": mined["top_fifo_values"][:8],
                "new_geo_edges": [
                    e for e in mined["edges"]
                    if not e["known_submit_site"] and e["n_geo_writes"] + e["n_fifo_writes"] > 0
                ][:12],
                "stream_push_candidates": [
                    e for e in mined["edges"] if e["looks_like_geo_stream_push"]
                ][:8],
            },
        })
    report = {
        "cases_run": ran,
        "policy": {
            "coverage_unit": "guest edge ip_before->ip_after",
            "max_steps_0": "not used (does not freeze)",
            "fail_closed": "no invented semantics; unknown paths not promoted",
        },
    }
    (OUT / "run_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    return report


def mine_existing() -> dict:
    ranges = load_recovered_ranges()
    sources = [
        ROOT / "out/attr-p1/pol_test_g7c60_97d.jsonl",
        ROOT / "out/attr-p1/pol_test_g7c60_985.jsonl",
        ROOT / "out/attr-p1/pol_test_g7c60_986.jsonl",
        ROOT / "out/attr-v0372/pol7c60-id985.jsonl",
        ROOT / "out/attr-v0372/pol7c60-id97d.jsonl",
    ]
    # also any probe_* we already produced
    sources.extend(sorted(OUT.glob("probe_*.jsonl")))
    results = []
    for src in sources:
        if not src.exists():
            continue
        if src.stat().st_size > 80_000_000:
            results.append({"path": str(src), "skipped": "too_large"})
            continue
        print(f"MINE {src}", flush=True)
        results.append(mine_trace(src, ranges))
    report = {"sources": results}
    (OUT / "mine_baseline.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    return report


def aggregate() -> dict:
    OUT.mkdir(parents=True, exist_ok=True)
    ranges = load_recovered_ranges()
    static_path = OUT / "static_rank.json"
    static = json.loads(static_path.read_text(encoding="utf-8")) if static_path.exists() else {}
    run_path = OUT / "run_report.json"
    run = json.loads(run_path.read_text(encoding="utf-8")) if run_path.exists() else {}
    mine_path = OUT / "mine_baseline.json"
    mine = json.loads(mine_path.read_text(encoding="utf-8")) if mine_path.exists() else {}

    # Collect all measured edges from run cases + mine sources
    all_edges: dict[tuple[str | None, str | None], dict] = {}

    def absorb(edge: dict, source: str) -> None:
        key = (edge.get("ip_before"), edge.get("ip_after"))
        rec = all_edges.get(key)
        if rec is None:
            all_edges[key] = {
                "ip_before": edge.get("ip_before"),
                "ip_after": edge.get("ip_after"),
                "witness_count": int(edge.get("witness_count") or 0),
                "write_addrs": list(edge.get("write_addrs") or []),
                "sample_values": list(edge.get("sample_values") or [])[:6],
                "n_geo_writes": int(edge.get("n_geo_writes") or 0),
                "n_fifo_writes": int(edge.get("n_fifo_writes") or 0),
                "known_submit_site": bool(edge.get("known_submit_site")),
                "looks_like_geo_stream_push": bool(edge.get("looks_like_geo_stream_push")),
                "sources": [source],
                "recovered_before": edge.get("recovered_before"),
                "recovered_after": edge.get("recovered_after"),
            }
        else:
            rec["witness_count"] += int(edge.get("witness_count") or 0)
            rec["n_geo_writes"] += int(edge.get("n_geo_writes") or 0)
            rec["n_fifo_writes"] += int(edge.get("n_fifo_writes") or 0)
            rec["looks_like_geo_stream_push"] = rec["looks_like_geo_stream_push"] or bool(
                edge.get("looks_like_geo_stream_push")
            )
            if source not in rec["sources"]:
                rec["sources"].append(source)
            for a in edge.get("write_addrs") or []:
                if a not in rec["write_addrs"]:
                    rec["write_addrs"].append(a)

    # Prefer all on-disk probe traces (run --filter overwrites run_report.json)
    probe_files = sorted(OUT.glob("probe_*.jsonl"))
    if probe_files:
        for jsonl in probe_files:
            mined = mine_trace(jsonl, ranges)
            for e in mined["edges"]:
                absorb(e, f"run:{jsonl.stem}")
    else:
        for case in run.get("cases_run") or []:
            name = case.get("name")
            jsonl = case.get("jsonl")
            if not jsonl or not Path(jsonl).exists():
                continue
            mined = mine_trace(Path(jsonl), ranges)
            for e in mined["edges"]:
                absorb(e, f"run:{name}")

    for src in (mine.get("sources") or []):
        if "edges" not in src:
            continue
        for e in src["edges"]:
            absorb(e, f"mine:{Path(src.get('path','')).name}")

    # Baseline known submit edges vs new
    baseline_edges = []
    new_edges = []
    for rec in all_edges.values():
        # fill recovered if missing
        if rec.get("ip_before") and rec["ip_before"].startswith("0x"):
            ip = int(rec["ip_before"], 16)
            rec["recovered_before"] = rec.get("recovered_before") or recovered_at(ip, ranges)
            rec["known_submit_site"] = rec["known_submit_site"] or is_known_submit_ip(ip)
        if rec.get("ip_after") and rec["ip_after"].startswith("0x"):
            ip = int(rec["ip_after"], 16)
            rec["recovered_after"] = rec.get("recovered_after") or recovered_at(ip, ranges)
        if rec["known_submit_site"]:
            baseline_edges.append(rec)
        else:
            if rec["n_geo_writes"] + rec["n_fifo_writes"] > 0:
                new_edges.append(rec)

    def sort_key(r: dict):
        new = 1 if not r["known_submit_site"] else 0
        push = 1 if r["looks_like_geo_stream_push"] else 0
        return (-push, -new, -r["witness_count"], r["ip_before"] or "")

    all_sorted = sorted(all_edges.values(), key=sort_key)
    new_sorted = sorted(new_edges, key=lambda r: (
        -int(r["looks_like_geo_stream_push"]),
        -r["witness_count"],
        r["ip_before"] or "",
    ))
    baseline_sorted = sorted(baseline_edges, key=lambda r: -r["witness_count"])

    stream_push = [r for r in all_sorted if r["looks_like_geo_stream_push"]]

    edge_list = {
        "coverage_unit": "guest edge ip_before->ip_after",
        "baseline_known_submit_ips_count": len(BASELINE_SUBMIT_IPS),
        "n_edges_total": len(all_sorted),
        "n_edges_baseline_known": len(baseline_sorted),
        "n_edges_new_geo_or_fifo": len(new_sorted),
        "n_geo_stream_push_candidates": len(stream_push),
        "top_new_edges": new_sorted[:40],
        "top_baseline_edges": baseline_sorted[:20],
        "stream_push_candidates": stream_push[:20],
        "all_edges": all_sorted,
        "static_top": (static.get("ranked_top") or [])[:20],
        "fail_closed": (
            "No class 0x01/02/05/07/09/0b/0c geo-stream push outside known "
            "submit sites is claimed unless looks_like_geo_stream_push is true "
            "AND witness writes are measured geo/FIFO stores. v0377 P2 "
            "sideband-port negative remains."
        ),
    }
    (OUT / "edge_list.json").write_text(json.dumps(edge_list, indent=2), encoding="utf-8")

    summary = {
        "n_edges_total": edge_list["n_edges_total"],
        "n_edges_new_geo_or_fifo": edge_list["n_edges_new_geo_or_fifo"],
        "n_geo_stream_push_candidates": edge_list["n_geo_stream_push_candidates"],
        "top_new": [
            {
                "edge": f"{e['ip_before']}->{e['ip_after']}",
                "witness_count": e["witness_count"],
                "write_addrs": e["write_addrs"][:6],
                "stream_push": e["looks_like_geo_stream_push"],
                "recovered_before": e.get("recovered_before"),
            }
            for e in new_sorted[:15]
        ],
        "static_call_7c60": static.get("counts", {}).get("call_7c60"),
        "static_call_7f24": static.get("counts", {}).get("call_7f24"),
    }
    (OUT / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    return edge_list


def capture_snippet(edge_key: str) -> dict:
    """Capture a minimized memory-trace snippet for a stream-push candidate."""
    ranges = load_recovered_ranges()
    edge_list_path = OUT / "edge_list.json"
    if not edge_list_path.exists():
        return {"error": "run aggregate first"}
    edge_list = json.loads(edge_list_path.read_text(encoding="utf-8"))
    target = None
    for e in edge_list.get("stream_push_candidates") or []:
        if edge_key in f"{e['ip_before']}->{e['ip_after']}":
            target = e
            break
    if target is None:
        for e in edge_list.get("top_new_edges") or []:
            if edge_key in f"{e['ip_before']}->{e['ip_after']}":
                target = e
                break
    if target is None:
        return {"error": f"edge not found: {edge_key}"}

    # Prefer a run probe that witnessed this edge
    run = json.loads((OUT / "run_report.json").read_text(encoding="utf-8")) if (OUT / "run_report.json").exists() else {}
    witness_jsonl = None
    witness_case = None
    for case in run.get("cases_run") or []:
        jsonl = case.get("jsonl")
        if not jsonl or not Path(jsonl).exists():
            continue
        mined = mine_trace(Path(jsonl), ranges)
        for e in mined["edges"]:
            if e["ip_before"] == target["ip_before"] and e["ip_after"] == target["ip_after"]:
                witness_jsonl = Path(jsonl)
                witness_case = case
                break
        if witness_jsonl:
            break
    if witness_jsonl is None:
        # search mine baseline
        mine_path = OUT / "mine_baseline.json"
        if mine_path.exists():
            mine = json.loads(mine_path.read_text(encoding="utf-8"))
            for src in mine.get("sources") or []:
                p = src.get("path")
                if not p or not Path(p).exists():
                    continue
                for e in src.get("edges") or []:
                    if e["ip_before"] == target["ip_before"] and e["ip_after"] == target["ip_after"]:
                        witness_jsonl = Path(p)
                        break
                if witness_jsonl:
                    break
    if witness_jsonl is None:
        return {"error": "no witness jsonl found", "target": target}

    # Extract a compact window: all geo/FIFO writes plus surrounding steps
    steps_of_interest = set()
    geo_events = []
    pending = []
    step_ip = {}
    with witness_jsonl.open("r", errors="replace") as f:
        for ln in f:
            if not ln.startswith("{"):
                continue
            try:
                j = json.loads(ln)
            except Exception:
                continue
            if j.get("type") == "step":
                step_ip[j.get("step")] = (j.get("ip_before"), j.get("ip_after"))
                if pending:
                    for mem in pending:
                        if mem.get("kind") != "write":
                            continue
                        addr = int(mem.get("address", 0))
                        if not is_geo_or_fifo(addr):
                            continue
                        ip_b, ip_a = step_ip.get(mem.get("step"), (None, None))
                        if ip_b == int(target["ip_before"], 16) or (
                            target["ip_before"] and ip_b == int(target["ip_before"], 16)
                        ):
                            try:
                                val = int.from_bytes(bytes.fromhex(mem.get("bytes") or "00")[:4], "little")
                            except Exception:
                                val = 0
                            geo_events.append({
                                "type": "memory",
                                "kind": "write",
                                "address": f"0x{addr:08x}",
                                "value": f"0x{val:08x}",
                                "step": mem.get("step"),
                                "ip_before": f"0x{ip_b:08x}" if ip_b is not None else None,
                                "ip_after": f"0x{ip_a:08x}" if ip_a is not None else None,
                            })
                            steps_of_interest.add(mem.get("step"))
                    pending = []
            elif j.get("type") == "memory":
                pending.append(j)

    # Also re-run a tiny probe if we know the case recipe and still no events
    snippet = {
        "target_edge": target,
        "witness_jsonl": str(witness_jsonl),
        "witness_case": witness_case.get("name") if witness_case else None,
        "n_geo_fifo_events": len(geo_events),
        "events": geo_events[:32],
        "classification": (
            "geo_stream_push_candidate"
            if target.get("looks_like_geo_stream_push")
            else "new_geo_or_fifo_write_edge_not_classified_as_stream_push"
        ),
        "fail_closed": (
            "Snippet lists measured writes only. Do not promote to TGP class "
            "0x01/02/05/07/09/0b/0c without independent packet evidence."
        ),
    }
    # Default snippet target: first stream-push or first new edge
    (OUT / f"snippet_{edge_key.replace(':', '_').replace('->', '_to_')}.json").write_text(
        json.dumps(snippet, indent=2), encoding="utf-8"
    )
    return snippet


def default_snippets() -> list[dict]:
    edge_list_path = OUT / "edge_list.json"
    if not edge_list_path.exists():
        return [{"error": "no edge_list.json"}]
    edge_list = json.loads(edge_list_path.read_text(encoding="utf-8"))
    targets = []
    for e in (edge_list.get("stream_push_candidates") or [])[:3]:
        targets.append(f"{e['ip_before']}->{e['ip_after']}")
    if not targets:
        for e in (edge_list.get("top_new_edges") or [])[:3]:
            targets.append(f"{e['ip_before']}->{e['ip_after']}")
    out = []
    for t in targets:
        key = t.replace("0x", "").replace("->", "-")
        print(f"SNIPPET {t}", flush=True)
        out.append(capture_snippet(t))
    (OUT / "snippets.json").write_text(json.dumps(out, indent=2), encoding="utf-8")
    return out


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("cmd", choices=["static", "mine", "run", "aggregate", "snippet", "all"])
    ap.add_argument("--filter", default=None)
    ap.add_argument("--edge", default=None)
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)

    if args.cmd in ("static", "all"):
        report = static_rank()
        (OUT / "static_rank.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(json.dumps({
            "call_7c60": report["counts"]["call_7c60"],
            "call_7f24": report["counts"]["call_7f24"],
            "imm_geo_port": report["counts"]["imm_geo_port"],
            "top": report["ranked_top"][:8],
        }, indent=2))
    if args.cmd in ("run", "all"):
        run_all(args.filter)
    if args.cmd in ("mine", "all"):
        mine_existing()
    if args.cmd in ("aggregate", "all"):
        edge_list = aggregate()
        print(json.dumps({
            "n_edges_total": edge_list["n_edges_total"],
            "n_edges_new_geo_or_fifo": edge_list["n_edges_new_geo_or_fifo"],
            "n_geo_stream_push_candidates": edge_list["n_geo_stream_push_candidates"],
        }, indent=2))
    if args.cmd == "snippet":
        if args.edge:
            print(json.dumps(capture_snippet(args.edge), indent=2))
        else:
            print(json.dumps(default_snippets(), indent=2))
    if args.cmd == "all":
        default_snippets()


if __name__ == "__main__":
    main()
