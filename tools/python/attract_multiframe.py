#!/usr/bin/env python3
"""Multi-frame resume-trace from a park, dumping state after each stop-at-a6c0."""
from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, wu8, wu32, tile_strings, WORK_BASE  # noqa: E402

I960 = Path("build/Debug/vf2i960.exe")
ROM = "roms/vf2"


def resume(snap: Path, out: Path, steps: int = 2000000, stop: str = "0x0000a6c0",
           write_addr: str | None = None, write_val: int | None = None) -> dict:
    cmd = [str(I960), "resume-trace", ROM, str(snap), str(steps),
           "0xffffffff", "0xffffffff"]
    if write_addr is not None and write_val is not None:
        cmd += [write_addr, str(write_val)]
    else:
        # pad to reach stop/output via argc paths — use 8-arg form: steps only + output
        pass
    # Use argc=11 form when we have write: rom snap steps clear ffff waddr wval output stop
    # argc=8: rom snap steps clear ffff output  — wait, argc 8 uses argv[7] as output without write
    if write_addr is None:
        # argc==8: argv[7]=output; stop not passed. Need argc 11 for stop without write.
        # argc 11 requires write_address at argv[7]. Use dummy write of current harmless value.
        cmd = [str(I960), "resume-trace", ROM, str(snap), str(steps),
               "0xffffffff", "0xffffffff", "0xffffffff", "0", str(out), stop]
        # write_address=0xffffffff means no write (UINT32_MAX check)
    else:
        cmd = [str(I960), "resume-trace", ROM, str(snap), str(steps),
               "0xffffffff", "0xffffffff", write_addr, str(write_val), str(out), stop]
    p = subprocess.run(cmd, capture_output=True, text=True)
    text = p.stdout + p.stderr
    rec = {"out": out.name, "rc": p.returncode}
    m = re.search(r"instructions=(\d+)", text.split("stop address")[-1] if "stop address" in text else text)
    # parse last IP=
    ips = re.findall(r"IP=0x([0-9a-fA-F]+)", text)
    insns = re.findall(r"instructions=(\d+)", text)
    frames = re.findall(r"frame-interrupts=(\d+)", text)
    rec["last_ip"] = int(ips[-1], 16) if ips else None
    rec["last_insns"] = int(insns[-1]) if insns else None
    rec["frame_irqs"] = int(frames[-1]) if frames else None
    rec["stopped"] = "stop address reached" in text
    if out.exists():
        snapd = parse_snap(out)
        work = snapd["regions"]["work-ram"]
        cfgbase = wu32(work, 0x50016C)
        rec.update({
            "ip": snapd["ip"],
            "sel": wu8(work, 0x50002A),
            "phase": wu8(work, 0x500030),
            "a4": wu8(work, 0x5000A4),
            "a6": wu8(work, 0x5000A6),
            "cd": wu32(work, 0x500024),
            "flags": wu32(work, 0x500068),
            "board": wu32(work, 0x508000),
            "country": wu8(work, cfgbase + 0x3350) if cfgbase >= WORK_BASE else None,
            "tiles": tile_strings(snapd["regions"]["tile-ram"], 90),
            "tex0": __import__("hashlib").sha256(snapd["regions"]["texture-ram0"]).hexdigest()[:16],
        })
        rec["is_test"] = rec["sel"] == 0x11 and rec["a4"] == 0x0B
    return rec


def main() -> None:
    start = Path(sys.argv[1])
    frames = int(sys.argv[2]) if len(sys.argv) > 2 else 6
    country = int(sys.argv[3]) if len(sys.argv) > 3 else None
    tag = sys.argv[4] if len(sys.argv) > 4 else "mf"
    outdir = Path("out/attr-mf")
    outdir.mkdir(parents=True, exist_ok=True)
    cur = start
    results = []
    for i in range(frames):
        out = outdir / f"{tag}-f{i:02d}.vf2snap"
        waddr = wval = None
        if i == 0 and country is not None:
            waddr, wval = "0x0059c350", country
        rec = resume(cur, out, write_addr=waddr, write_val=wval)
        rec["frame"] = i
        rec["from"] = cur.name
        results.append(rec)
        print(
            f"{tag} f{i:02d} {cur.name} -> {out.name} stop={rec.get('stopped')} "
            f"ip=0x{rec.get('ip',0):08x} sel=0x{rec.get('sel',0):02x} "
            f"ph=0x{rec.get('phase',0):02x} a4=0x{rec.get('a4',0):02x} "
            f"a6=0x{rec.get('a6',0):02x} country={rec.get('country')} "
            f"TEST={rec.get('is_test')} irq={rec.get('frame_irqs')} "
            f"tiles={rec.get('tiles','')[:70]!r}"
        )
        if not out.exists():
            break
        cur = out
        if rec.get("is_test"):
            print("  (TEST MENU reached — continuing one more frame anyway)")
    Path(outdir / f"{tag}-results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
