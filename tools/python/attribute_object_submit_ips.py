#!/usr/bin/env python3
"""Attribute object-table w0 writes / 0x7c60 call edges to guest IP in JSONL traces.

Correlates memory events to step.ip_before per AGENTS.md (memory.step is the
absolute upcoming instruction step; the following step record carries ip_before).
"""
from __future__ import annotations

import argparse
import json
import struct
from collections import Counter
from pathlib import Path

TABLE = 0x020E0004
BASE = 0x02000000
GEO_LO = 0x00800000
GEO_HI = 0x00808000
FIFO = 0x00884000
MAIN_DATA = Path("out/main_data.bin")


def load_w0_index(main_data: Path, count: int = 0x2000) -> dict[int, int]:
    blob = main_data.read_bytes()
    idx: dict[int, int] = {}
    for oid in range(count):
        off = (TABLE - BASE) + oid * 16
        if off + 16 > len(blob):
            break
        w0 = struct.unpack_from("<I", blob, off)[0]
        if w0 and w0 not in idx:
            idx[w0] = oid
    return idx


def scan(path: Path, w0_idx: dict[int, int]) -> dict:
    step_ip: dict[int, tuple[int | None, int | None]] = {}
    pending: list[dict] = []
    writes = []
    table_reads = []
    call_edges = Counter()
    call_samples = []

    def flush() -> None:
        nonlocal pending
        for mem in pending:
            step = mem.get("step")
            ip_b = step_ip.get(step, (None, None))[0]
            a = mem.get("address", 0)
            try:
                val = int.from_bytes(bytes.fromhex(mem.get("bytes") or "00")[:4], "little")
            except Exception:
                val = 0
            if mem.get("kind") == "write" and val in w0_idx:
                if a == FIFO or GEO_LO <= a < GEO_HI:
                    writes.append({
                        "address": f"0x{a:08x}",
                        "w0": f"0x{val:08x}",
                        "object_id": f"0x{w0_idx[val]:03x}",
                        "ip_before": f"0x{ip_b:08x}" if ip_b is not None else None,
                        "step": step,
                    })
            if mem.get("kind") == "read" and TABLE <= a < TABLE + 0x20000:
                if (a - TABLE) % 16 == 0:
                    table_reads.append({
                        "object_id": f"0x{(a - TABLE) // 16:03x}",
                        "addr": f"0x{a:08x}",
                        "ip_before": f"0x{ip_b:08x}" if ip_b is not None else None,
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
                pending.append(j)
                if j.get("step") in step_ip:
                    flush()
            elif t == "step":
                ip_b, ip_a = j.get("ip_before"), j.get("ip_after")
                step_ip[j["step"]] = (ip_b, ip_a)
                if pending and any(m.get("step") == j.get("step") for m in pending):
                    flush()
                if ip_a == 0x7C60:
                    site = f"0x{ip_b:08x}" if ip_b is not None else None
                    call_edges[site] += 1
                    if len(call_samples) < 16:
                        call_samples.append({"call_site": site, "step": j.get("step")})
        flush()

    return {
        "path": str(path),
        "n_w0_writes": len(writes),
        "writes_by_ip": dict(Counter(w["ip_before"] for w in writes).most_common(20)),
        "writes_by_id": dict(Counter(w["object_id"] for w in writes).most_common(40)),
        "write_samples": writes[:24],
        "n_table_reads": len(table_reads),
        "table_reads_by_ip": dict(Counter(r["ip_before"] for r in table_reads).most_common(12)),
        "table_reads_by_id": dict(Counter(r["object_id"] for r in table_reads).most_common(40)),
        "call_edges_7c60": dict(call_edges.most_common(20)),
        "call_samples": call_samples,
    }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("traces", nargs="+", type=Path)
    ap.add_argument("--main-data", type=Path, default=MAIN_DATA)
    args = ap.parse_args()
    w0_idx = load_w0_index(args.main_data)
    print(f"indexed {len(w0_idx)} unique object w0 values")
    for t in args.traces:
        if not t.exists():
            print(f"missing {t}")
            continue
        if t.stat().st_size > 200_000_000:
            print(f"skip large {t}")
            continue
        r = scan(t, w0_idx)
        print(json.dumps(r, indent=2))


if __name__ == "__main__":
    main()
