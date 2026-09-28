#!/usr/bin/env python3
"""Dump phase8 counters and related task pointers from attract parks."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, wu8, wu32, tile_strings, WORK_BASE


def main() -> None:
    for arg in sys.argv[1:]:
        p = Path(arg)
        if not p.exists():
            print("missing", p)
            continue
        s = parse_snap(p)
        w = s["regions"]["work-ram"]
        ptr = wu32(w, 0x500834)
        task0 = wu32(w, 0x500804)
        task1 = wu32(w, 0x500808)

        def read_u32(addr: int) -> int | None:
            if not (WORK_BASE <= addr < WORK_BASE + 0x100000 - 3):
                return None
            return wu32(w, addr)

        def read_u8(addr: int) -> int | None:
            if not (WORK_BASE <= addr < WORK_BASE + 0x100000):
                return None
            return wu8(w, addr)

        ctr = read_u32(ptr + 0x50) if ptr else None
        flags0 = read_u32(task0) if task0 else None
        flags1 = read_u32(task1) if task1 else None
        print(
            f"{p.name} ip=0x{s['ip']:08x} sel=0x{wu8(w,0x50002A):02x} "
            f"ph=0x{wu8(w,0x500030):02x} ready=0x{wu32(w,0x550000):08x} "
            f"flags68=0x{wu32(w,0x500068):08x} board=0x{wu32(w,0x508000):08x}"
        )
        print(
            f"  ptr50834=0x{ptr:08x} ptr+0x50={ctr} "
            f"task0=0x{task0:08x} f0=0x{flags0:08x} "
            f"task1=0x{task1:08x} f1=0x{flags1:08x}"
        )
        print(f"  mask28=0x{wu32(w,0x500028):08x} mode48=0x{wu8(w,0x500048):02x}")
        print(f"  tiles={tile_strings(s['regions']['tile-ram'], 70)!r}")


if __name__ == "__main__":
    main()
