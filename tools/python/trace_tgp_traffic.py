#!/usr/bin/env python3
"""Capture Model 2A TGP/geo traffic from a .vf2snap via vf2probe memory-trace."""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

PROBE = Path("build/Debug/vf2probe.exe")
ROM = "roms/vf2"

WATCH = {
    0x00980000: "copro_ctl",
    0x00980008: "geo_ctl",
    0x00880000: "copro_function",
    0x00884000: "copro_fifo",
    0x00800000: "geo_ram",
}


def main() -> None:
    snap = Path(sys.argv[1])
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("out/tgp-mtrace.jsonl")
    until = sys.argv[3] if len(sys.argv) > 3 else "0x00019024"
    steps = sys.argv[4] if len(sys.argv) > 4 else "400000"
    nav_clear = True
    cmd = [
        str(PROBE),
        "--rom-dir",
        ROM,
        "--snapshot",
        str(snap),
        "--until",
        until,
        "--max-steps",
        steps,
        "--memory-trace",
    ]
    if nav_clear:
        # probe set-u8 runs after restore; 0x500704 clear each... only once
        cmd.extend(["--set-u8", "0x00500704=0"])
    p = subprocess.run(cmd, capture_output=True, text=True)
    text = p.stdout + p.stderr
    out.write_text(text, encoding="utf-8", errors="replace")
    hits = []
    for ln in text.splitlines():
        if not ln.startswith("{"):
            continue
        try:
            j = json.loads(ln)
        except Exception:
            continue
        if j.get("type") != "memory":
            continue
        addr = j.get("address")
        if not isinstance(addr, int):
            continue
        # watch windows
        for base, name in WATCH.items():
            if base <= addr < base + 0x4000:
                hits.append((j.get("step"), j.get("kind"), name, addr, j.get("size"), j.get("bytes")))
                break
    print(f"wrote {out} lines={len(text.splitlines())} copro/geo_hits={len(hits)}")
    for h in hits[:80]:
        print(" ", h)
    if len(hits) > 80:
        print(f"  ... {len(hits)-80} more")


if __name__ == "__main__":
    main()
