#!/usr/bin/env python3
"""Phase14 thunk + long attract frame: who writes phase / TGP deltas."""
from __future__ import annotations

import json
import subprocess
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, wu8, wu32, tile_strings, sha16

PROBE = "build/Debug/vf2probe.exe"
I960 = "build/Debug/vf2i960.exe"
ROM = "roms/vf2"


def snap_line(path: Path) -> None:
    if not path.exists():
        print("  missing", path)
        return
    s = parse_snap(path)
    w = s["regions"]["work-ram"]
    g = s["regions"]["geometry"]
    x = s["regions"]["texture-ram0"]
    b = s["regions"]["buffer-ram"]
    t = s["regions"]["tile-ram"]
    print(
        f"  {path.name} ip=0x{s['ip']:08x} exec={s['executed']} "
        f"sel=0x{wu8(w,0x50002A):02x} ph=0x{wu8(w,0x500030):02x} "
        f"ready=0x{wu32(w,0x550000):08x} cd={wu32(w,0x500024)} "
        f"mask28=0x{wu32(w,0x500028):08x} nav=0x{wu32(w,0x500704):08x} "
        f"status55=0x{wu32(w,0x55C2F0):08x} tiles={tile_strings(t,40)!r}"
    )
    print(
        f"    geom={sha16(g)} buf={sha16(b)} tex={sha16(x)} "
        f"nz_tex={sum(1 for c in x[:0x10000] if c not in (0,0xFF))}"
    )


def probe(src, dst, sets, ip, until, steps, trace=False):
    cmd = [
        PROBE, "--rom-dir", ROM, "--snapshot", str(src),
        "--set-ip", ip, "--until", until, "--max-steps", steps,
        "--output-snapshot", str(dst),
    ]
    for s in sets:
        if s.startswith("u8:"):
            cmd += ["--set-u8", s[3:]]
        else:
            cmd += ["--set-u32", s]
    if trace:
        cmd.append("--memory-trace")
    p = subprocess.run(cmd, capture_output=True, text=True)
    return p.stdout + p.stderr


def classify_mem(text: str) -> None:
    addrs = Counter()
    fifo_vals = []
    geo_writes = 0
    proto_writes = 0
    phase_writes = 0
    ready_writes = 0
    for ln in text.splitlines():
        if not ln.startswith("{"):
            continue
        try:
            j = json.loads(ln)
        except Exception:
            continue
        if j.get("type") != "memory" or j.get("kind") != "write":
            continue
        a = j.get("address", 0)
        addrs[a] += 1
        if a in (0x00884000, 0x00880000, 0x00980000, 0x00980008):
            fifo_vals.append((hex(a), j.get("bytes", "")))
        if 0x00800000 <= a < 0x00900000:
            geo_writes += 1
        if a in (0x00500030, 0x00500031, 0x00500034):
            phase_writes += 1
            fifo_vals.append((hex(a), j.get("bytes", "")))
        if a == 0x00550000:
            ready_writes += 1
            fifo_vals.append((hex(a), j.get("bytes", "")))
    top = [(hex(a), n) for a, n in addrs.most_common(20)]
    print(f"    mem writes total_keys={len(addrs)} geo~{geo_writes} phase={phase_writes} ready={ready_writes}")
    print(f"    top writes: {top}")
    print(f"    interesting samples ({len(fifo_vals)}): {fifo_vals[:24]}")


def resume(src, dst, steps, stop="0x0000a6c0"):
    cmd = [
        I960, "resume-trace", ROM, str(src), str(steps),
        "0xffffffff", "0xffffffff",
        "0x00500704", "0", str(dst), stop,
    ]
    p = subprocess.run(cmd, capture_output=True, text=True)
    return p.stdout + p.stderr


def main() -> None:
    out = Path("out/attr-tail")
    out.mkdir(parents=True, exist_ok=True)
    src = out / "all0-ready1.vf2snap" if (out / "all0-ready1.vf2snap").exists() else Path("out/attr-fs/all0-ready1.vf2snap")
    # copy source availability
    if not src.exists():
        src = Path("out/attr-fs/all0-ready1.vf2snap")

    print("=== thunk 0x9444 with memory-trace (phase14 not-ready) ===")
    dst = out / "p14-thunk-trace.vf2snap"
    text = probe(
        src, dst,
        ["u8:0x00550000=0", "0x00515b50=0"],
        "0x0000c0a4", "0x00009444", "50",
        trace=True,
    )
    print(text[-300:])
    # save full trace to file if present in stdout jsonl
    trace_path = out / "p14-thunk.jsonl"
    lines = [ln for ln in text.splitlines() if ln.startswith("{")]
    if lines:
        trace_path.write_text("\n".join(lines) + "\n")
        print(f"    wrote {len(lines)} jsonl lines")
    classify_mem(text)
    snap_line(dst)

    print("\n=== continue thunk body 0x9444 -> 2000 steps +trace ===")
    dst2 = out / "p14-thunk-body.vf2snap"
    text2 = probe(
        src, dst2,
        ["u8:0x00550000=0", "0x00515b50=0"],
        "0x0000c0a4", "0x00009444", "50",
        trace=False,
    )
    # then run from 9444
    if dst.exists():
        text2 = probe(
            dst, dst2,
            [],
            "0x00009444", "0x0004d000", "2000",
            trace=True,
        )
        (out / "p14-thunk-body.jsonl").write_text(
            "\n".join(ln for ln in text2.splitlines() if ln.startswith("{")) + "\n"
        )
        print(text2[-250:])
        classify_mem(text2)
        snap_line(dst2)

    print("\n=== long resume from all0-ready1 (ready=0, nav=0, 400k) ===")
    long_out = out / "long-ready0.vf2snap"
    print(resume(src, long_out, "400000")[-500:])
    snap_line(long_out)

    print("\n=== multi-frame resume from p15-worker (phase16 park) ===")
    p15 = out / "p15-worker.vf2snap"
    if p15.exists():
        f15 = out / "p15w-f400k.vf2snap"
        print(resume(p15, f15, "400000")[-400:])
        snap_line(f15)

    print("\n=== multi-frame from p17-worker (phase0 wrap) ===")
    p17 = out / "p17-worker.vf2snap"
    if p17.exists():
        f17 = out / "p17w-f400k.vf2snap"
        print(resume(p17, f17, "400000")[-400:])
        snap_line(f17)

    print("\n=== texture/geom delta vs source ===")
    s0 = parse_snap(src)
    for name in ("p15-worker.vf2snap", "p16-worker-ctr1.vf2snap", "p17-worker.vf2snap",
                 "long-ready0.vf2snap", "p15w-f400k.vf2snap", "p17w-f400k.vf2snap"):
        p = out / name
        if not p.exists():
            continue
        s1 = parse_snap(p)
        t0 = s0["regions"]["texture-ram0"]
        t1 = s1["regions"]["texture-ram0"]
        g0 = s0["regions"]["geometry"]
        g1 = s1["regions"]["geometry"]
        b0 = s0["regions"]["buffer-ram"]
        b1 = s1["regions"]["buffer-ram"]
        dtex = sum(1 for a, b in zip(t0, t1) if a != b)
        dgeom = sum(1 for a, b in zip(g0, g1) if a != b)
        dbuf = sum(1 for a, b in zip(b0, b1) if a != b)
        print(f"  {name}: dtex={dtex} dgeom={dgeom} dbuf={dbuf} ph=0x{wu8(s1['regions']['work-ram'],0x500030):02x}")


if __name__ == "__main__":
    main()
