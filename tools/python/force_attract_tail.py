#!/usr/bin/env python3
"""Measure selector-3 tail (phases 14-17) on attract parks via oracle probe.

Fail-closed campaign tool: force phase byte / counters, run reference oracle
at frame dispatch or worker entry, dump measured state. Does not invent logo
semantics.
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, wu8, wu32, tile_strings, sha16

I960 = "build/Debug/vf2i960.exe"
PROBE = "build/Debug/vf2probe.exe"
ROM = "roms/vf2"
NAV = 0x00500704
PHASE = 0x00500030
MASK28 = 0x00500028
PTR_TASK = 0x00500834
PTR_T0 = 0x00500804
PTR_T1 = 0x00500808
PTR_T58 = 0x00500858
PTR_T64 = 0x00500864
CTR = 0x00515B50  # measured [0x500834]+0x50 when task=0x515b00
READY = 0x00550000


def run_probe(src: Path, dst: Path, sets: list[str], ip: str, until: str, steps: str) -> str:
    cmd = [
        PROBE,
        "--rom-dir", ROM,
        "--snapshot", str(src),
        "--set-ip", ip,
        "--until", until,
        "--max-steps", steps,
        "--output-snapshot", str(dst),
    ]
    for s in sets:
        if s.startswith("u8:"):
            cmd += ["--set-u8", s[3:]]
        else:
            cmd += ["--set-u32", s]
    p = subprocess.run(cmd, capture_output=True, text=True)
    return (p.stdout + p.stderr)[-800:]


def run_resume(src: Path, dst: Path, waddr: int, wval: int, steps: int, stop: str | None = None) -> str:
    cmd = [
        I960, "resume-trace", ROM, str(src), str(steps),
        "0xffffffff", "0xffffffff",
        hex(waddr), str(wval), str(dst),
    ]
    if stop:
        cmd.append(stop)
    p = subprocess.run(cmd, capture_output=True, text=True)
    return (p.stdout + p.stderr)[-800:]


def state(path: Path) -> str:
    if not path.exists():
        return f"  MISSING {path}"
    s = parse_snap(path)
    w = s["regions"]["work-ram"]
    g = s["regions"].get("geometry", b"")
    x = s["regions"].get("texture-ram0", b"")
    b = s["regions"].get("buffer-ram", b"")
    t = s["regions"].get("tile-ram", b"")
    task = wu32(w, PTR_TASK)
    t0 = wu32(w, PTR_T0)
    t1 = wu32(w, PTR_T1)
    flags0 = wu32(w, t0) if t0 and t0 >= 0x500000 else None
    flags1 = wu32(w, t1) if t1 and t1 >= 0x500000 else None
    ctr = wu32(w, task + 0x50) if task and task >= 0x500000 else None
    print(
        f"  {path.name} ip=0x{s['ip']:08x} exec={s['executed']} "
        f"sel=0x{wu8(w,0x50002A):02x} ph=0x{wu8(w,0x500030):02x} "
        f"ready=0x{wu32(w,READY):08x} mask28=0x{wu32(w,MASK28):08x} "
        f"nav=0x{wu32(w,NAV):08x} cd={wu32(w,0x500024)}"
    )
    print(
        f"    task=0x{task:08x} ctr+50={ctr} t0=0x{t0:08x} flags0={flags0} "
        f"t1=0x{t1:08x} flags1={flags1} "
        f"t58=0x{wu32(w,PTR_T58):08x} t64=0x{wu32(w,PTR_T64):08x} "
        f"status=0x{wu32(w,0x55C2F0):08x}"
    )
    print(
        f"    geom={sha16(g)} buf={sha16(b)} tex={sha16(x)} "
        f"nz_tex={sum(1 for c in x[:0x10000] if c not in (0,0xFF))} "
        f"tiles={tile_strings(t,40)!r}"
    )
    return ""


def main() -> None:
    src = Path(sys.argv[1] if len(sys.argv) > 1 else "out/attr-fs/all0-ready1.vf2snap")
    outdir = Path("out/attr-tail")
    outdir.mkdir(parents=True, exist_ok=True)
    print(f"SRC {src}")
    state(src)

    # Controlled matrix on oracle: force phase + worker/frame entries.
    cases = [
        ("p14-c0-ready0", "0x0000c0a4", ["u8:0x00550000=0", "0x00515b50=0"], "0x0000a010", "400"),
        ("p14-c0-ready0-9444", "0x0000c0a4", ["u8:0x00550000=0", "0x00515b50=0"], "0x00009444", "400"),
        ("p15-a6c0-mask1", "0x0000a6c0", ["u8:0x00500030=15", "u8:0x00500704=0", "0x00500028=0x00030001", "u8:0x00550000=0"], "0x0000a6c0", "120000"),
        ("p15-worker", "0x0000c268", ["u8:0x00500030=15", "0x00500028=0x00030001", "u8:0x00550000=0"], "0x0000a010", "800"),
        ("p16-worker-ctr1", "0x0000c414", ["u8:0x00500030=16", "0x00515b50=1", "u8:0x00550000=0"], "0x0000a010", "400"),
        ("p17-worker", "0x0000c448", ["u8:0x00500030=17", "u8:0x00500704=0", "u8:0x00550000=0"], "0x0000a010", "400"),
        ("p17-a6c0", "0x0000a6c0", ["u8:0x00500030=17", "u8:0x00500704=0", "u8:0x00550000=0"], "0x0000a6c0", "80000"),
        ("p0-a6c0-after17", "0x0000a6c0", ["u8:0x00500030=0", "u8:0x00500704=0", "u8:0x00550000=0", "0x00500028=0x00030100"], "0x0000a6c0", "80000"),
    ]
    for name, ip, sets, until, steps in cases:
        dst = outdir / f"{name}.vf2snap"
        print(f"\n=== {name} ip={ip} until={until} steps={steps}")
        print(run_probe(src, dst, sets, ip, until, steps)[-400:])
        state(dst)

    # Resume-trace frame from forced phase15 park if produced.
    p15 = outdir / "p15-a6c0-mask1.vf2snap"
    if p15.exists():
        dst = outdir / "p15-frame.vf2snap"
        print("\n=== resume p15 frame nav=0 stop a6c0")
        print(run_resume(p15, dst, NAV, 0, 200000, "0x0000a6c0")[-400:])
        state(dst)

    # Compare geometry buffer-ram heads.
    print("\n=== geometry/buffer first nz dwords (src)")
    s = parse_snap(src)
    g = s["regions"]["geometry"]
    nz = [(i, int.from_bytes(g[i:i+4], "little")) for i in range(0, min(len(g), 0x8000), 4)
          if int.from_bytes(g[i:i+4], "little") != 0]
    print(f"  nz_dwords={len(nz)} first12={[(hex(a), hex(v)) for a,v in nz[:12]]}")


if __name__ == "__main__":
    main()
