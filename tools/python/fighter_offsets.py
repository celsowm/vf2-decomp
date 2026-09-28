#!/usr/bin/env python3
"""Fighter-relative offset ranking for frontier traces (analysis only)."""
from __future__ import annotations

import argparse
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path


def parse_hex_list(values):
    out = []
    for v in values or []:
        out.append(int(str(v), 0))
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("trace")
    ap.add_argument("--fighter-base", action="append", default=[])
    ap.add_argument("--window", type=lambda s: int(s, 0), default=0x2000)
    ap.add_argument("--limit", type=int, default=40)
    args = ap.parse_args()
    bases = parse_hex_list(args.fighter_base)
    if not bases:
        print("provide at least one --fighter-base", file=sys.stderr)
        return 2
    # offset -> base_count, reads, writes, ips, widths
    stats = {}
    for line in open(args.trace, encoding="utf-8"):
        if '"type":"memory"' not in line:
            continue
        try:
            r = json.loads(line)
        except json.JSONDecodeError:
            continue
        addr = int(r.get("address", 0))
        kind = str(r.get("kind", "read"))
        size = int(r.get("size", 0) or 0)
        for base in bases:
            if base <= addr < base + args.window:
                off = addr - base
                key = off
                rec = stats.setdefault(
                    key,
                    {
                        "offset": off,
                        "bases": set(),
                        "reads": 0,
                        "writes": 0,
                        "sizes": Counter(),
                    },
                )
                rec["bases"].add(base)
                rec["sizes"][size] += 1
                if kind == "write":
                    rec["writes"] += 1
                else:
                    rec["reads"] += 1
                break
    rows = []
    for rec in stats.values():
        rows.append(
            {
                "offset": f"+0x{rec['offset']:04x}",
                "base_count": len(rec["bases"]),
                "reads": rec["reads"],
                "writes": rec["writes"],
                "total": rec["reads"] + rec["writes"],
                "sizes": dict(rec["sizes"]),
            }
        )
    rows.sort(key=lambda r: (-r["base_count"], -r["total"], r["offset"]))
    print(json.dumps(rows[: args.limit], indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
