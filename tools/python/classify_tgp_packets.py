#!/usr/bin/env python3
"""Classify TGP/geometry memory-trace packets for polygon-ROM object addrs."""
from __future__ import annotations

import json
import sys
from collections import Counter
from pathlib import Path


def classify(path: Path) -> None:
    fifo = []
    geo = []
    func = Counter()
    obj_rom = []
    obj_ram = []
    uploads = 0
    for ln in path.read_text(errors="replace").splitlines():
        if not ln.startswith("{"):
            continue
        try:
            j = json.loads(ln)
        except Exception:
            continue
        if j.get("type") != "memory" or j.get("kind") != "write":
            continue
        a = j.get("address", 0)
        try:
            bs = bytes.fromhex(j.get("bytes", "00"))
            val = int.from_bytes(bs[:4], "little")
        except Exception:
            val = 0
        if a == 0x00884000:
            fifo.append(val)
            cls = (val >> 23) & 0x1F
            func[cls] += 1
        elif 0x00800000 <= a < 0x00808000:
            geo.append((a, val))
        elif a in (0x00980000, 0x00880000):
            uploads += 1
            func[f"port_{a:08x}"] += 1
    print(f"{path.name}: fifo_writes={len(fifo)} geo_writes={len(geo)} port_writes={uploads}")
    print(f"  fifo command_class(top)={func.most_common(16)}")
    # If class-0x01 object commands were fully streamed we'd see 5-word packets;
    # single-word FIFO writes make reconstruction incomplete — note that.
    if fifo:
        print(f"  fifo sample words={[hex(v) for v in fifo[:12]]}")
        # heuristic: words with bit31+0x800000 patterns
        romish = [v for v in fifo if (v & 0x00800000) != 0 and (v >> 24) in (0x00, 0x01, 0x02)]
        print(f"  words with poly-rom bit among low 24-bit objs: {len(romish)} sample={[hex(v) for v in romish[:8]]}")
    if geo:
        print(f"  geo sample={[(hex(a), hex(v)) for a,v in geo[:8]]}")


def main() -> None:
    args = [Path(a) for a in sys.argv[1:]]
    if not args:
        args = list(Path("out/attr-long").glob("fifo*.jsonl"))[:6]
        args += list(Path("out/attr-phase-writes").glob("*.jsonl"))[:8]
        args += list(Path("out/attr-v0371").glob("*.jsonl"))[:6]
    for p in args:
        if p.exists() and p.stat().st_size < 80_000_000:
            classify(p)
        elif p.exists():
            print(f"skip large {p} {p.stat().st_size}")


if __name__ == "__main__":
    main()
