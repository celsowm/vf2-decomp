#!/usr/bin/env python3
"""Stream FIFO/geo memory-trace JSONL and extract MEASURED TGP transform state.

Evidence-first: only records values that actually appear as Model 2A
memory events. Does not invent camera matrices. Fail-closed for logo.

TGP class encoding (src/hardware/tgp.c vf2_tgp_execute_geometry_stream):
  class = (command[0] >> 23) & 0x1f
  0x01 object (command[3]=object_address)
  0x02 direct geometry
  0x07 geometry_mode = command[1]
  0x09 focus_x=float(command[1]), focus_y=float(command[2])
  0x0b full 3x4 matrix via geometry_write_matrix
  0x0c translate matrix[12,13,14]
  0x0f/0x1f end
"""
from __future__ import annotations

import argparse
import json
import struct
from collections import Counter
from pathlib import Path
from typing import Any, Iterable, Iterator

TABLE = 0x020E0004
TABLE_LO = 0x020E0000
TABLE_HI = 0x020F0000
FIFO_LO = 0x00884000
FIFO_HI = 0x00886000
GEO_LO = 0x00800000
GEO_HI = 0x00810000
APERTURE_CURSOR = 0x005001E4
MAIN_DATA = Path("out/main_data.bin")
DEFAULT_TRACES = [
    Path("out/attr-long/fifo-phase5.jsonl"),
    Path("out/attr-long/fifo-attract.jsonl"),
    Path("out/attr-v0372/boot-sel03-fifo.jsonl"),
    Path("out/attr-v0372/boot-sel09-fifo.jsonl"),
    Path("out/attr-v0372/boot-sel09-long.jsonl"),
    Path("out/attr-logo/emit31040.jsonl"),
]
MAX_EVENTS = 400
GEO_DUMP_WORDS = 64
FIFO_DUMP_WORDS = 64
# Measured FIFO protocol immediates (logo_object_submit_v0372 / attract notes).
# These are NOT TGP class commands even when bits 23-27 collide with 07/09/0b/0c.
KNOWN_PROTOCOL_WORDS = {
    0x00800101,
    0x01800303,
    0x03000606,
    0x1A003434,
    0x14802929,
    0x1C803939,
    0x09801313,
    0xFFFFFFFF,
}


def is_protocol_like(val: int) -> bool:
    """Heuristic: FIFO color/marker immediates, not TGP stream opcodes.

    Observed family: 0x??80YYZZ / 0x??00YYZZ with YY often == ZZ
    (0x33806767, 0x34806969, 0x14802929, ...). Bit23-27 collisions with
    TGP classes are accidental.
    """
    if val in KNOWN_PROTOCOL_WORDS:
        return True
    if val == 0 or val == 0xFFFFFFFF:
        return True
    b0 = val & 0xFF
    b1 = (val >> 8) & 0xFF
    b2 = (val >> 16) & 0xFF
    b3 = (val >> 24) & 0xFF
    # Repeated low bytes + marker 0x80/0x00 in third byte → color-like tag.
    if b0 == b1 and b2 in (0x00, 0x80) and b3 != 0:
        return True
    # Pure opcode-like word0 for class 07/09/0b/0c: low bits small/structured.
    return False


def looks_like_tgp_opcode(val: int) -> bool:
    """Conservative: opcode word0 with class 07/09/0b/0c and not protocol-like."""
    cls = tgp_class(val)
    if cls not in (0x07, 0x09, 0x0B, 0x0C):
        return False
    if is_protocol_like(val):
        return False
    # Opcode candidates keep low 23 bits modest (flags/payload small) rather
    # than color-tag structure.
    low = val & 0x7FFFFF
    if low <= 0x7FFFF and (val & 0x00FF0000) == 0:
        return True
    # Also accept when bit31 clear and top class field isolated.
    return (val & 0x80000000) == 0 and not is_protocol_like(val)


def u32le(hex_bytes: str | None) -> int:
    if not hex_bytes:
        return 0
    try:
        bs = bytes.fromhex(hex_bytes)[:4]
    except ValueError:
        return 0
    if not bs:
        return 0
    return int.from_bytes(bs, "little")


def f32(bits: int) -> float:
    return struct.unpack("<f", struct.pack("<I", bits & 0xFFFFFFFF))[0]


def tgp_class(word0: int) -> int:
    return (word0 >> 23) & 0x1F


def matrix_from_0b_words(words: list[int]) -> list[float] | None:
    """geometry_write_matrix(tgp.c): command[0]=class, then 12 payload words."""
    if len(words) < 13:
        return None
    w = words
    m = [0.0] * 16
    m[0] = f32(w[1])
    m[4] = f32(w[4])
    m[8] = f32(w[7])
    m[12] = f32(w[10])
    m[1] = f32(w[2])
    m[5] = f32(w[5])
    m[9] = f32(w[8])
    m[13] = f32(w[11])
    m[2] = f32(w[3])
    m[6] = f32(w[6])
    m[10] = f32(w[9])
    m[14] = f32(w[12])
    m[3] = 0.0
    m[7] = 0.0
    m[11] = 0.0
    m[15] = 1.0
    return m


def load_w0_index(main_data: Path, count: int = 0x2000) -> dict[int, int]:
    if not main_data.exists():
        return {}
    blob = main_data.read_bytes()
    idx: dict[int, int] = {}
    for oid in range(count):
        off = (TABLE - 0x02000000) + oid * 16
        if off + 16 > len(blob):
            break
        w0 = struct.unpack_from("<I", blob, off)[0]
        if w0 and w0 not in idx:
            idx[w0] = oid
    return idx


def iter_jsonl(path: Path) -> Iterator[dict[str, Any]]:
    with path.open("r", errors="replace") as fh:
        for line in fh:
            if not line.startswith("{"):
                continue
            try:
                yield json.loads(line)
            except json.JSONDecodeError:
                continue


class StreamState:
    def __init__(self, w0_index: dict[int, int]) -> None:
        self.w0_index = w0_index
        self.class_hist: Counter[int] = Counter()
        self.fifo_writes = 0
        self.geo_writes = 0
        self.geo_port_writes = 0  # 0x800010 / 0x804000 family
        self.table_reads = 0
        self.aperture_events = 0
        self.commands: list[dict[str, Any]] = []
        self.matrix_events: list[dict[str, Any]] = []
        self.mode_events: list[dict[str, Any]] = []
        self.object_events: list[dict[str, Any]] = []
        self.live = {
            "geometry_mode": None,
            "focus_x": None,
            "focus_y": None,
            "matrix": None,
        }
        self.geo_port_words: list[int] = []
        self.fifo_word_sample: list[int] = []
        self.pending_window: list[tuple[int, int, int]] = []  # (step, addr, val)
        self.step_ip: dict[int, tuple[int | None, int | None]] = {}
        self.raw_class_hits: Counter[str] = Counter()
        self.protocol_false_class: Counter[str] = Counter()
        self.tgp_opcode_candidates: list[dict[str, Any]] = []
        self.sources: list[dict[str, Any]] = []
        self.w0_hits: Counter[int] = Counter()

    def note_event(self, kind: str, ev: dict[str, Any], port: str) -> None:
        if len(self.commands) + len(self.matrix_events) + len(self.mode_events) < MAX_EVENTS:
            rec = {
                "class": kind,
                "port": port,
                "step": ev.get("step"),
                "addr": f"0x{ev['addr']:08x}",
                "w0": f"0x{ev['val']:08x}",
            }
            if "focus_x" in ev:
                rec["focus_x"] = ev["focus_x"]
                rec["focus_y"] = ev["focus_y"]
            if "mode" in ev:
                rec["mode"] = ev["mode"]
            if "matrix" in ev:
                rec["matrix"] = ev["matrix"]
            if "words" in ev:
                rec["words"] = [f"0x{w:08x}" for w in ev["words"]]
            if "ip_before" in ev:
                rec["ip_before"] = ev["ip_before"]
            if kind.startswith("0x09"):
                self.commands.append(rec)
            elif kind.startswith("0x0b") or kind.startswith("0x0c"):
                self.matrix_events.append(rec)
            elif kind.startswith("0x07"):
                self.mode_events.append(rec)
            else:
                self.commands.append(rec)

    def classify_word(self, step: int, addr: int, val: int, port: str, ip_before: int | None) -> None:
        cls = tgp_class(val)
        self.class_hist[cls] += 1
        port_label = f"{port}:0x{addr:08x}"
        if port == "geo_port":
            if len(self.geo_port_words) < GEO_DUMP_WORDS * 8:
                self.geo_port_words.append(val)
        elif port == "fifo":
            if len(self.fifo_word_sample) < FIFO_DUMP_WORDS * 4:
                self.fifo_word_sample.append(val)

        # Classify the word itself as a potential TGP command word0.
        if cls in (0x07, 0x09, 0x0B, 0x0C) and is_protocol_like(val):
            # FIFO color/marker immediate whose bits collide with TGP classes.
            self.protocol_false_class[f"0x{cls:02x}"] += 1
            return
        if cls == 0x07:
            self.raw_class_hits["0x07"] += 1
            # Mode is command[1]; try nearby pending window for a following word.
            nxt = self._peek_following(step, addr)
            mode = nxt if nxt is not None else None
            if mode is not None:
                self.live["geometry_mode"] = mode
            self.note_event(
                "0x07",
                {
                    "step": step,
                    "addr": addr,
                    "val": val,
                    "mode": mode,
                    "ip_before": f"0x{ip_before:08x}" if ip_before is not None else None,
                },
                port_label,
            )
        elif cls == 0x09:
            self.raw_class_hits["0x09"] += 1
            if looks_like_tgp_opcode(val):
                self.tgp_opcode_candidates.append(
                    {"class": "0x09", "w0": f"0x{val:08x}", "step": step, "port": port}
                )
            follow = self._peek_following_n(step, addr, 2)
            if len(follow) >= 2:
                fx, fy = f32(follow[0]), f32(follow[1])
                self.live["focus_x"] = fx
                self.live["focus_y"] = fy
                self.note_event(
                    "0x09",
                    {
                        "step": step,
                        "addr": addr,
                        "val": val,
                        "focus_x": fx,
                        "focus_y": fy,
                        "words": [val, follow[0], follow[1]],
                        "ip_before": f"0x{ip_before:08x}" if ip_before is not None else None,
                    },
                    port_label,
                )
            else:
                self.note_event(
                    "0x09",
                    {
                        "step": step,
                        "addr": addr,
                        "val": val,
                        "words": [val],
                        "note": "class-09 word0 without measured follow words",
                        "ip_before": f"0x{ip_before:08x}" if ip_before is not None else None,
                    },
                    port_label,
                )
        elif cls == 0x0B:
            self.raw_class_hits["0x0b"] += 1
            if looks_like_tgp_opcode(val):
                self.tgp_opcode_candidates.append(
                    {"class": "0x0b", "w0": f"0x{val:08x}", "step": step, "port": port}
                )
            follow = self._peek_following_n(step, addr, 12)
            words = [val] + follow
            if len(words) >= 13:
                m = matrix_from_0b_words(words)
                self.live["matrix"] = m
                self.note_event(
                    "0x0b",
                    {
                        "step": step,
                        "addr": addr,
                        "val": val,
                        "matrix": m,
                        "words": words[:13],
                        "ip_before": f"0x{ip_before:08x}" if ip_before is not None else None,
                    },
                    port_label,
                )
            else:
                self.note_event(
                    "0x0b",
                    {
                        "step": step,
                        "addr": addr,
                        "val": val,
                        "words": words,
                        "note": "class-0b word0 without full 12 follow words",
                        "ip_before": f"0x{ip_before:08x}" if ip_before is not None else None,
                    },
                    port_label,
                )
        elif cls == 0x0C:
            self.raw_class_hits["0x0c"] += 1
            if looks_like_tgp_opcode(val):
                self.tgp_opcode_candidates.append(
                    {"class": "0x0c", "w0": f"0x{val:08x}", "step": step, "port": port}
                )
            follow = self._peek_following_n(step, addr, 3)
            if len(follow) >= 3:
                tx, ty, tz = f32(follow[0]), f32(follow[1]), f32(follow[2])
                mat = self.live.get("matrix")
                if mat is None:
                    mat = [1.0, 0.0, 0.0, 0.0,
                           0.0, 1.0, 0.0, 0.0,
                           0.0, 0.0, 1.0, 0.0,
                           tx, ty, tz, 1.0]
                    self.live["matrix"] = mat
                else:
                    mat = list(mat)
                    mat[12], mat[13], mat[14] = tx, ty, tz
                    self.live["matrix"] = mat
                self.note_event(
                    "0x0c",
                    {
                        "step": step,
                        "addr": addr,
                        "val": val,
                        "matrix": mat,
                        "translate": [tx, ty, tz],
                        "words": [val] + follow,
                        "ip_before": f"0x{ip_before:08x}" if ip_before is not None else None,
                    },
                    port_label,
                )
            else:
                self.note_event(
                    "0x0c",
                    {
                        "step": step,
                        "addr": addr,
                        "val": val,
                        "words": [val],
                        "note": "class-0c word0 without measured translate words",
                        "ip_before": f"0x{ip_before:08x}" if ip_before is not None else None,
                    },
                    port_label,
                )

        # Object submission: write value equals known object-table w0.
        if port in ("fifo", "geo", "geo_port") and val in self.w0_index and val != 0xFFFFFFFF:
            oid = self.w0_index[val]
            self.w0_hits[val] += 1
            if len(self.object_events) < MAX_EVENTS:
                self.object_events.append(
                    {
                        "id": f"0x{oid:03x}",
                        "w0": f"0x{val:08x}",
                        "addr": f"0x{addr:08x}",
                        "port": port,
                        "step": step,
                        "ip_before": f"0x{ip_before:08x}" if ip_before is not None else None,
                    }
                )

        # Keep a small recent word window for follow-word lookup.
        self.pending_window.append((step, addr, val))
        if len(self.pending_window) > 64:
            self.pending_window.pop(0)

    def _peek_following(self, step: int, addr: int) -> int | None:
        follow = self._peek_following_n(step, addr, 1)
        return follow[0] if follow else None

    def _peek_following_n(self, step: int, addr: int, n: int) -> list[int]:
        """Look for subsequent words after (step,addr) in the same pending window.

        Memory events use absolute upcoming instruction step; correlate by step
        then by later steps / sequential addresses when present.
        """
        out: list[int] = []
        seen_self = False
        for s, a, v in self.pending_window:
            if not seen_self:
                if s == step and a == addr:
                    seen_self = True
                continue
            # Prefer later steps, or sequential geo addresses after a port write.
            if s >= step or a > addr:
                out.append(v)
            if len(out) >= n:
                break
        return out[:n]

    def record_table_read(self, step: int, addr: int, val: int, ip_before: int | None) -> None:
        self.table_reads += 1
        oid = (addr - TABLE) // 16
        if len(self.object_events) < MAX_EVENTS:
            self.object_events.append(
                {
                    "id": f"0x{oid:03x}",
                    "addr": f"0x{addr:08x}",
                    "kind": "table_read",
                    "step": step,
                    "value": f"0x{val:08x}",
                    "ip_before": f"0x{ip_before:08x}" if ip_before is not None else None,
                }
            )

    def snapshot_reads(self) -> dict[str, Any]:
        has_matrix = self.live.get("matrix") is not None
        has_focus = self.live.get("focus_x") is not None
        has_mode = self.live.get("geometry_mode") is not None
        measured = has_matrix or has_focus or has_mode
        if measured:
            conf = "measured"
            note = (
                "Live transform fields updated from measured class-07/09/0b/0c "
                "words in FIFO/geo memory events. Unfilled fields remain null; "
                "do not invent defaults for host camera."
            )
        else:
            conf = "absent"
            note = (
                "No class-0x07/0x09/0x0b/0x0c TGP transform stream commands measured "
                "as raw FIFO/geo write word0 in these traces after excluding FIFO "
                "protocol/color immediates (0x14802929 / 0x33806767 family). "
                "IEEE-like FIFO words 0x43850000/0xc3850000 collide with class-07 "
                "bits but have no measured command[1] mode payload. Object "
                "submissions and protocol constants remain present. Host raster "
                "must not invent a game camera; fail-closed for logo naming. "
                "See work_ram_display_triples for measured display-state triple "
                "(6.0,4.7,18.5) and camera scale 0x501084/0x501088=0x44160000."
            )
        return {
            "geometry_mode": self.live.get("geometry_mode"),
            "focus_x": self.live.get("focus_x"),
            "focus_y": self.live.get("focus_y"),
            "matrix": self.live.get("matrix"),
            "confidence": conf,
            "note": note,
        }


def classify_port(addr: int) -> str | None:
    if FIFO_LO <= addr < FIFO_HI:
        return "fifo"
    if addr in (0x00800010, 0x00804000):
        return "geo_port"
    if GEO_LO <= addr < GEO_HI:
        return "geo"
    return None


def scan_trace(path: Path, state: StreamState) -> dict[str, Any]:
    lines = 0
    mem_writes = 0
    fifo_before = state.fifo_writes
    geo_before = state.geo_writes
    geo_port_before = state.geo_port_writes
    fifo_class_sample = Counter()
    for ev in iter_jsonl(path):
        lines += 1
        t = ev.get("type")
        if t == "step":
            state.step_ip[ev.get("step")] = (ev.get("ip_before"), ev.get("ip_after"))
            continue
        if t != "memory":
            continue
        step = ev.get("step")
        addr = ev.get("address", 0)
        kind = ev.get("kind")
        val = u32le(ev.get("bytes"))
        ip_b = state.step_ip.get(step, (None, None))[0]
        if kind == "write":
            mem_writes += 1
            port = classify_port(addr)
            if port is None:
                continue
            if port == "fifo":
                state.fifo_writes += 1
            elif port == "geo_port":
                state.geo_port_writes += 1
            else:
                state.geo_writes += 1
            fifo_class_sample[tgp_class(val)] += 1
            state.classify_word(step or -1, addr, val, port, ip_b)
        elif kind == "read" and TABLE_LO <= addr < TABLE_HI and (addr - TABLE) % 16 == 0:
            state.record_table_read(step or -1, addr, val, ip_b)
        elif kind in ("read", "write") and addr == APERTURE_CURSOR:
            state.aperture_events += 1
    state.sources.append(
        {
            "path": str(path).replace("\\", "/"),
            "lines": lines,
            "memory_writes": mem_writes,
            "fifo_writes": state.fifo_writes - fifo_before,
            "geo_writes": state.geo_writes - geo_before,
            "geo_port_writes": state.geo_port_writes - geo_port_before,
        }
    )
    return {"lines": lines, "fifo_classes": fifo_class_sample}


def try_fifo_stream_parse(words: list[int]) -> dict[str, Any]:
    """Alternate hypothesis: FIFO write sequence as packed TGP command stream."""
    events = {
        "class_07": 0,
        "class_09": 0,
        "class_0b": 0,
        "class_0c": 0,
        "parse_errors": 0,
        "commands": 0,
        "samples": [],
    }
    i = 0
    n = len(words)
    while i < n:
        w0 = words[i]
        if w0 & 0x80000000:
            events["parse_errors"] += 1
            i += 1
            continue
        cls = tgp_class(w0)
        # Minimal length table from tgp.c geometry_count_words for transform classes.
        req = {0x00: 1, 0x07: 2, 0x08: 2, 0x09: 3, 0x0A: 4, 0x0B: 13,
               0x0C: 4, 0x0D: 3, 0x0F: 1, 0x1F: 1, 0x01: 5}.get(cls)
        if req is None:
            events["parse_errors"] += 1
            i += 1
            continue
        if i + req > n:
            events["parse_errors"] += 1
            break
        chunk = words[i : i + req]
        events["commands"] += 1
        if cls == 0x07:
            events["class_07"] += 1
        elif cls == 0x09:
            events["class_09"] += 1
        elif cls == 0x0B:
            events["class_0b"] += 1
        elif cls == 0x0C:
            events["class_0c"] += 1
        if cls in (0x07, 0x09, 0x0B, 0x0C) and len(events["samples"]) < 32:
            events["samples"].append(
                {"class": f"0x{cls:02x}", "words": [f"0x{x:08x}" for x in chunk]}
            )
        i += req
        if cls in (0x0F, 0x1F):
            break
    return events


def dump_snap_regions(paths: list[Path]) -> list[dict[str, Any]]:
    """Read-only snapshot dumps via dump_attract_state parser (no probe exec)."""
    import sys

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    out = []
    for p in paths:
        if not p.exists():
            out.append({"path": str(p), "error": "missing"})
            continue
        try:
            from dump_attract_state import parse_snap, wu32, wu8  # type: ignore

            snap = parse_snap(p)
            work = snap["regions"].get("work-ram", b"")
            geom = snap["regions"].get("geometry", b"")
            copro = snap["regions"].get("copro-port", b"")
            rec: dict[str, Any] = {
                "path": str(p).replace("\\", "/"),
                "ip": f"0x{snap['ip']:08x}",
                "executed": snap["executed"],
                "geom_region_len": len(geom),
                "copro_port_len": len(copro),
                "note": "park-state from snap parser; probe --max-steps is not a freeze",
            }
            if len(work) > (0x005001E4 - 0x00500000) + 4:
                rec["work_0x5001e4"] = f"0x{wu32(work, 0x005001E4):08x}"
            # Measured Work-RAM display/runtime pointers (evidence, not camera claim).
            for addr in (0x0050084C, 0x00500804, 0x00500808, 0x00501084, 0x00501088):
                if len(work) >= addr - 0x00500000 + 4:
                    raw = wu32(work, addr)
                    rec[f"work_0x{addr:08x}"] = f"0x{raw:08x}"
                    if addr in (0x00501084, 0x00501088):
                        rec[f"work_0x{addr:08x}_f32"] = f32(raw)
            # If 0x50084c holds a work pointer, dump +0x40 flags, +0x54..+0x60 triples.
            disp_ptr = None
            if len(work) >= 0x84C + 4:
                disp_ptr = wu32(work, 0x0050084C)
            if disp_ptr and 0x00500000 <= disp_ptr < 0x00600000:
                base = disp_ptr - 0x00500000
                def wt(off: int) -> int | None:
                    o = base + off
                    if 0 <= o + 4 <= len(work):
                        return wu32(work, disp_ptr + off)
                    return None
                rec["display_object"] = {
                    "ptr": f"0x{disp_ptr:08x}",
                    "flags_40": None if wt(0x40) is None else f"0x{wt(0x40):08x}",
                    "x54": None if wt(0x54) is None else f32(wt(0x54)),  # type: ignore[arg-type]
                    "y58": None if wt(0x58) is None else f32(wt(0x58)),  # type: ignore[arg-type]
                    "z5c": None if wt(0x5C) is None else f32(wt(0x5C)),  # type: ignore[arg-type]
                    "x54_bits": None if wt(0x54) is None else f"0x{wt(0x54):08x}",
                    "y58_bits": None if wt(0x58) is None else f"0x{wt(0x58):08x}",
                    "z5c_bits": None if wt(0x5C) is None else f"0x{wt(0x5C):08x}",
                    "x60_bits": None if wt(0x60) is None else f"0x{wt(0x60):08x}",
                    "note": (
                        "Work-RAM display-object fields at measured pointer from "
                        "0x50084c. +0x54.. is display transform triple per "
                        "display_runtime_followup_v0026 — NOT a TGP 3x4 matrix."
                    ),
                }
            # First words of snapshot geometry region (read-only hex dump).
            words = []
            for i in range(0, min(len(geom), 256), 4):
                words.append(f"0x{struct.unpack_from('<I', geom, i)[0]:08x}")
            rec["geometry_region_words"] = words
            nz_words = []
            for i in range(0, min(len(geom), 0x8000), 4):
                w = struct.unpack_from("<I", geom, i)[0]
                if w != 0:
                    nz_words.append((f"0x{i:04x}", f"0x{w:08x}"))
            rec["geometry_region_nz_words"] = len(nz_words)
            rec["geometry_region_nz_list"] = nz_words[:80]
            out.append(rec)
        except Exception as exc:  # fail-closed: record error, do not invent
            out.append({"path": str(p).replace("\\", "/"), "error": str(exc)})
    return out


def probe_read_regions(snaps: list[Path], rom_dir: Path, probe: Path) -> list[dict[str, Any]]:
    """Optional read-only vf2probe --read-u32 dumps (no --set-*, no output-snap)."""
    import subprocess

    addrs = [
        0x005001E4,
        0x00800000,
        0x00800010,
        0x00804000,
        0x00884000,
        0x00800004,
        0x00800008,
        0x0080000C,
    ]
    out = []
    if not probe.exists():
        return [{"error": f"probe missing: {probe}"}]
    for snap in snaps:
        if not snap.exists():
            out.append({"path": str(snap), "error": "missing"})
            continue
        cmd = [str(probe), "--rom-dir", str(rom_dir), "--snapshot", str(snap), "--max-steps", "0"]
        for a in addrs:
            cmd.extend(["--read-u32", f"0x{a:08x}"])
        try:
            p = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        except Exception as exc:
            out.append({"path": str(snap).replace("\\", "/"), "error": str(exc)})
            continue
        text = p.stdout + "\n" + p.stderr
        reads = []
        for line in text.splitlines():
            if "read" in line.lower() or "0x" in line:
                if line.strip():
                    reads.append(line.strip())
        out.append(
            {
                "path": str(snap).replace("\\", "/"),
                "exit": p.returncode,
                "stdout_tail": text[-2000:],
                "note": "read-only probe dump; no --set-* / no output snapshot claimed",
            }
        )
    return out


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("traces", nargs="*", type=Path, default=None)
    ap.add_argument("--output", type=Path, default=Path("out/attr-render/transforms_phase5.json"))
    ap.add_argument("--main-data", type=Path, default=MAIN_DATA)
    ap.add_argument("--snaps", nargs="*", type=Path, default=[
        Path("out/attr-long/long-29.vf2snap"),
        Path("out/attr-v0372/boot-sel03-fifo.vf2snap"),
        Path("out/attr-logo/emit31040.vf2snap"),
    ])
    ap.add_argument("--probe", type=Path, default=Path("build/Debug/vf2probe.exe"))
    ap.add_argument("--rom-dir", type=Path, default=Path("roms/vf2"))
    ap.add_argument("--skip-probe", action="store_true")
    args = ap.parse_args()
    traces = args.traces or [p for p in DEFAULT_TRACES if p.exists()]
    if not traces:
        traces = list(Path("out/attr-long").glob("fifo*.jsonl"))[:4]

    w0_index = load_w0_index(args.main_data)
    state = StreamState(w0_index)
    per_trace_class: dict[str, Any] = {}
    for t in traces:
        if not t.exists():
            state.sources.append({"path": str(t), "error": "missing"})
            continue
        print(f"[extract] {t} ({t.stat().st_size} bytes)", flush=True)
        info = scan_trace(t, state)
        per_trace_class[str(t).replace("\\", "/")] = {
            "lines": info["lines"],
            "class_hist_top": [[f"0x{k:02x}" if isinstance(k, int) else k, v]
                               for k, v in info["fifo_classes"].most_common(24)],
        }

    fifo_stream = try_fifo_stream_parse(state.fifo_word_sample)
    geo_stream = try_fifo_stream_parse(state.geo_port_words)

    snap_dumps = dump_snap_regions(list(args.snaps))
    probe_dumps: list[dict[str, Any]] = []
    if not args.skip_probe:
        probe_dumps = probe_read_regions(list(args.snaps), args.rom_dir, args.probe)

    live = state.snapshot_reads()
    # Refine live_guess with stream-parse evidence if word-level was absent.
    if live["confidence"] == "absent":
        if fifo_stream.get("class_09") or fifo_stream.get("class_0b") or fifo_stream.get("class_0c") or \
           geo_stream.get("class_09") or geo_stream.get("class_0b") or geo_stream.get("class_0c"):
            live["confidence"] = "measured"
            live["note"] = (
                live["note"]
                + " Alternate FIFO/geo packed-stream parse DID find transform "
                "classes; inspect stream_parse_* samples."
            )
        elif state.tgp_opcode_candidates:
            live["note"] = (
                live["note"]
                + f" Non-protocol opcode-like word0 candidates="
                f"{len(state.tgp_opcode_candidates)} but follow words incomplete; "
                "live matrix/focus remain null (fail-closed)."
            )

    # Park-state Work-RAM display triples (measured from snaps; not TGP matrix).
    work_display = []
    for s in snap_dumps:
        if "display_object" in s:
            work_display.append({"path": s.get("path"), **s["display_object"]})

    top_objects = [
        {
            "id": f"0x{w0_index[k]:03x}" if k in w0_index else None,
            "w0": f"0x{k:08x}",
            "n": v,
        }
        for k, v in state.w0_hits.most_common(40)
    ]

    result = {
        "sources": state.sources,
        "per_trace_class_hist": per_trace_class,
        "counts": {
            "fifo_writes": state.fifo_writes,
            "geo_writes": state.geo_writes,
            "geo_port_writes": state.geo_port_writes,
            "table_reads": state.table_reads,
            "aperture_0x5001e4_events": state.aperture_events,
            "raw_class_hits": dict(state.raw_class_hits),
            "protocol_false_class": dict(state.protocol_false_class),
            "tgp_opcode_candidates": state.tgp_opcode_candidates[:64],
            "class_hist_all_fifo_geo": {f"0x{k:02x}": v for k, v in sorted(state.class_hist.items())},
            "object_w0_hit_unique": len(state.w0_hits),
        },
        "commands": state.commands,
        "matrix_events": state.matrix_events,
        "mode_events": state.mode_events,
        "object_events": state.object_events,
        "object_w0_top": top_objects,
        "live_guess": live,
        "stream_parse_fifo_sample": fifo_stream,
        "stream_parse_geo_port_sample": geo_stream,
        "geo_port_word_sample_hex": [f"0x{w:08x}" for w in state.geo_port_words[:GEO_DUMP_WORDS]],
        "fifo_word_sample_hex": [f"0x{w:08x}" for w in state.fifo_word_sample[:FIFO_DUMP_WORDS]],
        "snapshot_dumps": snap_dumps,
        "work_ram_display_triples": work_display,
        "probe_read_dumps": probe_dumps,
        "fail_closed": {
            "logo_named": False,
            "camera_invented": False,
            "tgp_matrix_measured": live.get("confidence") == "measured" and live.get("matrix") is not None,
            "fifo_protocol_false_classes_excluded": True,
            "note": (
                "Measured-only transform extraction. FIFO color/marker immediates "
                "with colliding class bits (0x33806767/0x14802929 family) are "
                "classified as protocol, not TGP 07/09/0b/0c commands. Object IDs "
                "are numeric. No 3D logo claim. Hybrid fa_camera recovery is not "
                "applied unless live memory dumps show corresponding values. "
                "Probe --max-steps dumps are post-execution; park-state comes from "
                "snap parser only."
            ),
        },
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(f"[wrote] {args.output}")
    print(json.dumps({
        "live_guess": live,
        "counts": result["counts"],
        "fifo_stream_classes": {k: fifo_stream.get(k) for k in ("class_07", "class_09", "class_0b", "class_0c", "parse_errors")},
        "geo_stream_classes": {k: geo_stream.get(k) for k in ("class_07", "class_09", "class_0b", "class_0c", "parse_errors")},
        "n_object_events": len(state.object_events),
        "protocol_false_class": result["counts"]["protocol_false_class"],
        "tgp_opcode_candidates_n": len(state.tgp_opcode_candidates),
        "work_ram_display_triples": work_display,
        "geo_port_sample": result["geo_port_word_sample_hex"][:16],
        "fifo_sample": result["fifo_word_sample_hex"][:16],
    }, indent=2))


if __name__ == "__main__":
    main()
