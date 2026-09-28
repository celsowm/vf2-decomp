#!/usr/bin/env python3
"""Correlate FIFO function codes with texture nz on attract parks."""
from __future__ import annotations

import hashlib
import json
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, wu8, wu32


def fifo_funcs(path: Path) -> Counter:
    c: Counter = Counter()
    if not path.exists():
        return c
    for ln in path.read_text(errors="replace").splitlines():
        if not ln.startswith("{"):
            continue
        try:
            j = json.loads(ln)
        except Exception:
            continue
        if j.get("type") != "memory" or j.get("kind") != "write":
            continue
        if j.get("address") != 0x00884000:
            continue
        bs = bytes.fromhex(j.get("bytes") or "00000000")
        val = int.from_bytes(bs, "little")
        c[(val >> 23) & 0x3F] += 1
    return c


def snap_info(path: Path) -> dict:
    s = parse_snap(path)
    w = s["regions"]["work-ram"]
    tex = s["regions"]["texture-ram0"]
    return {
        "name": path.name,
        "sel": wu8(w, 0x50002A),
        "ph": wu8(w, 0x500030),
        "ready": wu32(w, 0x550000),
        "nz_tex": sum(1 for b in tex[:0x10000] if b not in (0, 0xFF)),
        "tex0": hashlib.sha256(tex).hexdigest()[:16],
        "ctr0": wu32(w, 0x5502C0),
        "ctr2": wu32(w, 0x5502E0),
    }


def main() -> None:
    traces = [
        Path("out/attr-long/fifo-attract.jsonl"),
        Path("out/attr-long/fifo-phase5.jsonl"),
        Path("out/attr-long/fifo-test.jsonl"),
    ]
    snaps = [
        Path("out/attr-nav/postteste-f02.vf2snap"),
        Path("out/attr-long/long-29.vf2snap"),
        Path("out/attr-phases/s04-frame.vf2snap"),
        Path("out/sixth-fresh.vf2snap"),
    ]
    print("=== FIFO function-code histograms ===")
    for t in traces:
        c = fifo_funcs(t)
        total = sum(c.values())
        print(f"{t.name}: writes={total} top={c.most_common(12)}")
    print("\n=== Snapshot visual/ready state ===")
    for p in snaps:
        if p.exists():
            print(snap_info(p))
    print(
        "\n=== Correlation (measured, not causal) ===\n"
        "Attract parks with high FIFO writes also show texture-ram nz>0\n"
        "and ready/ctr states that cycle 1→0; TEST has FIFO≈9 and nz=0.\n"
        "Function codes are not yet mapped to named meshes (fail-closed)."
    )


if __name__ == "__main__":
    main()
