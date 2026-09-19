#!/usr/bin/env python3
"""Multi-frame attract with nav 0x500704 forced clear each frame."""
from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, wu8, wu32, tile_strings, WORK_BASE

I960 = "build/Debug/vf2i960.exe"
ROM = "roms/vf2"
NAV = "0x00500704"
INP = "0x00500700"
IDLE = 0x0FF7F700


def resume(cur: Path, out: Path, write_addr: str, write_val: int, steps: int = 4000000) -> dict:
    cmd = [
        I960, "resume-trace", ROM, str(cur), str(steps),
        "0xffffffff", "0xffffffff", write_addr, str(write_val),
        str(out), "0x0000a6c0",
    ]
    p = subprocess.run(cmd, capture_output=True, text=True)
    text = p.stdout + p.stderr
    rec = {"out": out.name, "stopped": "stop address reached" in text}
    ips = re.findall(r"IP=0x([0-9a-fA-F]+)", text)
    rec["halt_ip"] = int(ips[-1], 16) if ips else None
    frames = re.findall(r"frame-interrupts=(\d+)", text)
    rec["irq"] = int(frames[-1]) if frames else None
    if out.exists():
        s = parse_snap(out)
        w = s["regions"]["work-ram"]
        cfg = wu32(w, 0x50016C)
        rec.update({
            "ip": s["ip"],
            "sel": wu8(w, 0x50002A),
            "phase": wu8(w, 0x500030),
            "a4": wu8(w, 0x5000A4),
            "a6": wu8(w, 0x5000A6),
            "nav704": wu32(w, 0x500704),
            "inp700": wu32(w, 0x500700),
            "cd": wu32(w, 0x500024),
            "flags": wu32(w, 0x500068),
            "board": wu32(w, 0x508000),
            "country": wu8(work := w, cfg + 0x3350) if cfg >= WORK_BASE else None,
            "tiles": tile_strings(s["regions"]["tile-ram"], 100),
            "tex0": __import__("hashlib").sha256(s["regions"]["texture-ram0"]).hexdigest()[:16],
            "geom": __import__("hashlib").sha256(s["regions"]["geometry"]).hexdigest()[:16],
        })
        rec["is_test"] = rec["sel"] == 0x11 and rec["a4"] == 0x0B
        rec["nav_gate"] = bool(rec["nav704"] & ((1 << 26) | (1 << 2)))
    return rec


def main() -> None:
    start = Path(sys.argv[1] if len(sys.argv) > 1 else "out/sega-after-cd.vf2snap")
    nframes = int(sys.argv[2]) if len(sys.argv) > 2 else 8
    tag = sys.argv[3] if len(sys.argv) > 3 else "navclear"
    # alternate clear target: nav vs input idle pattern
    mode = sys.argv[4] if len(sys.argv) > 4 else "nav"
    outdir = Path("out/attr-nav")
    outdir.mkdir(parents=True, exist_ok=True)
    cur = start
    results = []
    for i in range(nframes):
        out = outdir / f"{tag}-f{i:02d}.vf2snap"
        if mode == "nav":
            waddr, wval = NAV, 0
        elif mode == "idle":
            waddr, wval = INP, IDLE
        elif mode == "both":
            # only one write per resume-trace — use nav this frame
            waddr, wval = NAV, 0
        else:
            waddr, wval = NAV, 0
        rec = resume(cur, out, waddr, wval)
        rec["frame"] = i
        results.append(rec)
        print(
            f"{tag} f{i:02d} stop={rec.get('stopped')} ip=0x{rec.get('ip',0):08x} "
            f"sel=0x{rec.get('sel',0):02x} ph=0x{rec.get('phase',0):02x} "
            f"a4=0x{rec.get('a4',0):02x} nav=0x{rec.get('nav704',0):08x} "
            f"inp=0x{rec.get('inp700',0):08x} gate={rec.get('nav_gate')} "
            f"TEST={rec.get('is_test')} irq={rec.get('irq')} "
            f"tex={rec.get('tex0')} tiles={rec.get('tiles','')[:80]!r}"
        )
        if not out.exists():
            break
        cur = out
        if rec.get("is_test"):
            print("  TEST reached")
            break
    Path(outdir / f"{tag}-results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    print("wrote", outdir / f"{tag}-results.json")


if __name__ == "__main__":
    main()
