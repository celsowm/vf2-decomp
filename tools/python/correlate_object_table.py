#!/usr/bin/env python3
"""Correlate attract/TGP traces with object table word0 hits at 0x020e0004."""
from __future__ import annotations

import json
import struct
import sys
from collections import Counter
from pathlib import Path

ROM_DIR = Path("roms/vf2")
TABLE = 0x020E0004
BASE = 0x02000000
PAIRS = [
    ("mpr-17560.10", 0x00000000), ("mpr-17561.11", 0x00000002),
    ("mpr-17558.8", 0x00400000), ("mpr-17559.9", 0x00400002),
    ("mpr-17566.6", 0x00800000), ("mpr-17567.7", 0x00800002),
    ("mpr-17564.4", 0x00C00000), ("mpr-17565.5", 0x00C00002),
]


def build_main_data() -> bytes:
    region = bytearray(0x02400000)
    for name, off in PAIRS:
        p = ROM_DIR / name
        if not p.exists():
            continue
        src = p.read_bytes()
        for i in range(0, len(src), 2):
            dest = off + (i // 2) * 4
            if dest + 1 < len(region):
                region[dest] = src[i]
                region[dest + 1] = src[i + 1]
    return bytes(region)


def load_object_index(img: bytes, count: int = 0x200) -> dict[int, int]:
    """word0 -> object id (first match); also word1/word2 maps."""
    w0, w1, w2 = {}, {}, {}
    for obj in range(count):
        off = (TABLE - BASE) + obj * 16
        if off + 16 > len(img):
            break
        a, b, c, d = struct.unpack_from("<IIII", img, off)
        if a and a not in w0:
            w0[a] = obj
        if b and b not in w1:
            w1[b] = obj
        if c and c not in w2:
            w2[c] = obj
    return {"w0": w0, "w1": w1, "w2": w2}


def scan_trace(path: Path, idx: dict) -> None:
    hits = Counter()
    table_reads = Counter()
    fifo_n = geo_n = 0
    samples = []
    for ln in path.read_text(errors="replace").splitlines():
        if not ln.startswith("{"):
            continue
        try:
            j = json.loads(ln)
        except Exception:
            continue
        if j.get("type") != "memory":
            continue
        a = j.get("address", 0)
        kind = j.get("kind")
        try:
            val = int.from_bytes(bytes.fromhex(j.get("bytes", "00"))[:4], "little")
        except Exception:
            val = 0
        if kind == "read" and TABLE <= a < TABLE + 0x2000:
            table_reads[a] += 1
        if kind != "write":
            continue
        if a == 0x00884000:
            fifo_n += 1
        if 0x00800000 <= a < 0x00808000 or a == 0x00884000:
            for name, m in (("w0", idx["w0"]), ("w1", idx["w1"]), ("w2", idx["w2"])):
                if val in m:
                    hits[(name, m[val], hex(val))] += 1
                    if len(samples) < 24:
                        samples.append((hex(a), name, hex(m[val]), hex(val)))
    print(f"{path.name}: fifo~{fifo_n} obj_hits={len(hits)} table_reads={len(table_reads)}")
    if hits:
        print("  top object ids:", hits.most_common(16))
        print("  samples:", samples[:12])
    if table_reads:
        print("  table read addrs:", [(hex(a), n) for a, n in table_reads.most_common(10)])


def main() -> None:
    img = build_main_data()
    idx = load_object_index(img)
    print(f"indexed w0={len(idx['w0'])} w1={len(idx['w1'])} w2={len(idx['w2'])}")
    paths = []
    for pat in (
        "out/attr-long/fifo-attract.jsonl",
        "out/attr-long/fifo-phase5.jsonl",
        "out/attr-logo/*.jsonl",
        "out/attr-phase-writes/*.jsonl",
        "out/attr-v0371/*.jsonl",
    ):
        paths.extend(Path().glob(pat))
    for p in paths:
        if p.exists() and 0 < p.stat().st_size < 80_000_000:
            scan_trace(p, idx)


if __name__ == "__main__":
    main()
