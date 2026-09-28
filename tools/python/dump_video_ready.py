#!/usr/bin/env python3
"""Dump video/texture ready-related words from attract parks."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, wu8, wu32, WORK_BASE


def main() -> None:
    for arg in sys.argv[1:]:
        p = Path(arg)
        if not p.exists():
            print("missing", p)
            continue
        s = parse_snap(p)
        w = s["regions"]["work-ram"]

        def u32(a):
            return wu32(w, a) if WORK_BASE <= a < WORK_BASE + 0x100000 - 3 else None

        print(
            f"{p.name} ip=0x{s['ip']:08x} sel=0x{wu8(w,0x50002A):02x} "
            f"ph=0x{wu8(w,0x500030):02x} ready=0x{u32(0x550000):08x}"
        )
        for name, addr in (
            ("ctr0 5502c0", 0x5502C0),
            ("ctr1 5502d0", 0x5502D0),
            ("ctr2 5502e0", 0x5502E0),
            ("pkt+4", 0x5502E4),
            ("pkt+8", 0x5502E8),
            ("pkt+c", 0x5502EC),
            ("55000c", 0x55000C),
            ("55c2f0 status", 0x55C2F0),
            ("ctr task 515b50", 0x515B50),
        ):
            v = u32(addr)
            print(f"  {name}=0x{v:08x}" if v is not None else f"  {name}=?")
        nz55 = sum(
            1
            for i in range(0, 0x400, 4)
            if u32(0x550000 + i) not in (0, None)
        )
        print(f"  work 0x550000..+0x400 nz_words={nz55}")


if __name__ == "__main__":
    main()
