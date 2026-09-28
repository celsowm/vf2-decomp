#!/usr/bin/env python3
"""Correlate long-29 memory-trace object writes with instruction IPs."""
from __future__ import annotations

import json
import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from render_poly_objects import build, MAIN_PAIRS, TABLE, BASE


def ip_for(step_ip, step):
    if step in step_ip:
        return step_ip[step]
    if step is None:
        return None
    for d in (1, -1, 2, -2, 3, -3, 4, 5):
        if (step + d) in step_ip:
            return step_ip[step + d]
    return None


def main() -> None:
    p = Path(sys.argv[1] if len(sys.argv) > 1 else "out/attr-logo/long29-obj.jsonl")
    step_ip = {}
    mem = []
    table_reads = []
    for ln in p.open(errors="replace"):
        if not ln.startswith("{"):
            continue
        try:
            j = json.loads(ln)
        except Exception:
            continue
        if j.get("type") == "step":
            step_ip[j.get("step")] = j.get("ip_before")
        if j.get("type") == "memory":
            mem.append(j)
            a = j.get("address", 0)
            if j.get("kind") == "read" and 0x20E0000 <= a < 0x2100000:
                table_reads.append((j.get("step"), a))

    ids = Counter()
    for step, a in table_reads:
        if a >= TABLE and (a - TABLE) % 16 == 0:
            obj = (a - TABLE) // 16
            ids[obj] += 1
    print("table reads", len(table_reads))
    print("object ids from table reads:", [(hex(i), n) for i, n in ids.most_common(24)])

    print("\nobject w0 geo writes with nearby ip:")
    for j in mem:
        if j.get("type") != "memory" or j.get("kind") != "write":
            continue
        a = j.get("address", 0)
        if a not in (0x800010, 0x804000):
            continue
        try:
            val = int.from_bytes(bytes.fromhex(j.get("bytes", "00"))[:4], "little")
        except Exception:
            continue
        if not (0x8E000 <= val <= 0xC0000 or 0x4A0000 <= val <= 0x4B0000):
            continue
        ip = ip_for(step_ip, j.get("step"))
        ips = hex(ip) if ip is not None else None
        print(f"  {hex(a)} val={val:#x} step={j.get('step')} ip={ips}")

    img = build(MAIN_PAIRS, 0x02400000)
    print("\ntable for measured ids:")
    for obj in sorted(ids):
        off = (TABLE - BASE) + obj * 16
        if off + 16 > len(img):
            print(f"  id={obj:#x} OOB")
            continue
        words = struct.unpack_from("<IIII", img, off)
        print(f"  id=0x{obj:03x} {[hex(w) for w in words]}")


if __name__ == "__main__":
    main()
