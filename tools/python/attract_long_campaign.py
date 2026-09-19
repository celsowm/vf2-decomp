#!/usr/bin/env python3
"""Long attract campaign: multi-step resume-trace + FIFO differential attract vs TEST."""
from __future__ import annotations

import hashlib
import json
import re
import struct
import subprocess
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, wu8, wu32, tile_strings, WORK_BASE

I960 = "build/Debug/vf2i960.exe"
PROBE = "build/Debug/vf2probe.exe"
ROM = "roms/vf2"


def sha16(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()[:16]


def resume(cur: Path, out: Path, steps: int, nav: int = 0) -> dict:
    cmd = [
        I960, "resume-trace", ROM, str(cur), str(steps),
        "0xffffffff", "0xffffffff",
        "0x00500704", str(nav),
        str(out), "0x0000a6c0",
    ]
    p = subprocess.run(cmd, capture_output=True, text=True)
    text = p.stdout + p.stderr
    rec = {
        "out": out.name,
        "stopped": "stop address reached" in text,
        "unsupported": "unsupported operation" in text,
    }
    m = re.findall(r"instructions=(\d+)", text)
    rec["insns"] = int(m[-1]) if m else None
    m = re.findall(r"frame-interrupts=(\d+)", text)
    rec["irq"] = int(m[-1]) if m else None
    m = re.findall(r"IP=0x([0-9a-fA-F]+)", text)
    rec["halt_ip"] = int(m[-1], 16) if m else None
    if out.exists():
        s = parse_snap(out)
        w = s["regions"]["work-ram"]
        cfg = wu32(w, 0x50016C)
        tex = s["regions"]["texture-ram0"]
        geom = s["regions"]["geometry"]
        tile = s["regions"]["tile-ram"]
        nz = sum(1 for c in tex[:0x10000] if c not in (0, 0xFF))
        rec.update({
            "ip": s["ip"],
            "exec": s["executed"],
            "sel": wu8(w, 0x50002A),
            "phase": wu8(w, 0x500030),
            "a4": wu8(w, 0x5000A4),
            "cd": wu32(w, 0x500024),
            "nav": wu32(w, 0x500704),
            "board": wu32(w, 0x508000),
            "flags": wu32(w, 0x500068),
            "country": wu8(w, cfg + 0x3350) if cfg >= WORK_BASE else None,
            "nz_tex64k": nz,
            "tex0": sha16(tex),
            "geom": sha16(geom),
            "tiles": tile_strings(tile, 80),
            "is_test": wu8(w, 0x50002A) == 0x11 and wu8(w, 0x5000A4) == 0x0B,
        })
    return rec


def parse_fifo(trace_path: Path) -> dict:
    funcs: Counter[int] = Counter()
    addrs: Counter[str] = Counter()
    writes = reads = 0
    samples = []
    if not trace_path.exists():
        return {"error": "missing trace"}
    for ln in trace_path.read_text(errors="replace").splitlines():
        if not ln.startswith("{"):
            continue
        try:
            j = json.loads(ln)
        except Exception:
            continue
        if j.get("type") != "memory":
            continue
        addr = j.get("address")
        kind = j.get("kind")
        if addr == 0x00884000:
            if kind == "write":
                writes += 1
                bs = bytes.fromhex(j.get("bytes") or "00000000")
                val = int.from_bytes(bs, "little")
                fn = (val >> 23) & 0x3F
                funcs[fn] += 1
                if len(samples) < 24:
                    samples.append(f"{val:08x}")
            elif kind == "read":
                reads += 1
        elif isinstance(addr, int) and kind == "write":
            if 0x00980000 <= addr < 0x00980040:
                addrs[f"copro_ctl:{addr:08x}"] += 1
            elif 0x00880000 <= addr < 0x00884000:
                addrs[f"copro_fn:{addr:08x}"] += 1
            elif 0x00800000 <= addr < 0x00804000:
                addrs["geo"] += 1
    return {
        "fifo_writes": writes,
        "fifo_reads": reads,
        "func_top": funcs.most_common(20),
        "other_writes": addrs.most_common(15),
        "samples": samples,
    }


def probe_trace(snap: Path, out: Path, until: str, steps: str, nav: bool = True) -> Path:
    cmd = [PROBE, "--rom-dir", ROM, "--snapshot", str(snap),
           "--until", until, "--max-steps", steps, "--memory-trace"]
    if nav:
        cmd += ["--set-u8", "0x00500704=0"]
    p = subprocess.run(cmd, capture_output=True, text=True)
    out.write_text(p.stdout + p.stderr, encoding="utf-8", errors="replace")
    return out


def main() -> None:
    mode = sys.argv[1] if len(sys.argv) > 1 else "long"
    outdir = Path("out/attr-long")
    outdir.mkdir(parents=True, exist_ok=True)

    if mode == "long":
        cur = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("out/attr-nav/postteste-f02.vf2snap")
        n = int(sys.argv[3]) if len(sys.argv) > 3 else 20
        steps = int(sys.argv[4]) if len(sys.argv) > 4 else 8000000
        results = []
        for i in range(n):
            out = outdir / f"long-{i:02d}.vf2snap"
            rec = resume(cur, out, steps, nav=0)
            rec["i"] = i
            results.append(rec)
            print(
                f"L{i:02d} stop={rec.get('stopped')} unsup={rec.get('unsupported')} "
                f"ip=0x{rec.get('ip',0):08x} sel=0x{rec.get('sel',0):02x} "
                f"ph=0x{rec.get('phase',0):02x} cd={rec.get('cd')} nav=0x{rec.get('nav',0):08x} "
                f"nz_tex={rec.get('nz_tex64k')} TEST={rec.get('is_test')} "
                f"irq={rec.get('irq')} insns={rec.get('insns')} "
                f"tex={rec.get('tex0')} tiles={rec.get('tiles','')[:40]!r}",
                flush=True,
            )
            if not out.exists():
                break
            if rec.get("is_test") or rec.get("unsupported"):
                print("  terminal condition", flush=True)
                break
            cur = out
        (outdir / "long-results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
        return

    if mode == "fifo":
        # attract vs TEST differential
        cases = [
            ("attract", Path("out/attr-nav/postteste-f02.vf2snap"), "0x00019024", "250000", True),
            ("attract_long", Path("out/attr-long/long-00.vf2snap") if Path("out/attr-long/long-00.vf2snap").exists() else Path("out/attr-nav/postteste-long.vf2snap"), "0x0004cf90", "200000", True),
            ("test_menu", Path("out/sixth-fresh.vf2snap"), "0x0001645c", "200000", False),
        ]
        summary = {}
        for name, snap, until, steps, nav in cases:
            if not snap.exists():
                print(f"skip {name} missing {snap}")
                continue
            tr = probe_trace(snap, outdir / f"fifo-{name}.jsonl", until, steps, nav)
            info = parse_fifo(tr)
            summary[name] = info
            print(f"\n== {name} {snap.name}")
            print(f"  fifo_w={info.get('fifo_writes')} fifo_r={info.get('fifo_reads')}")
            print(f"  func_top={info.get('func_top')}")
            print(f"  other={info.get('other_writes')}")
            print(f"  samples={info.get('samples')[:12]}")
        (outdir / "fifo-summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
        return

    if mode == "witness":
        for p in sys.argv[2:]:
            path = Path(p)
            if not path.exists():
                continue
            s = parse_snap(path)
            w = s["regions"]["work-ram"]
            tex = s["regions"]["texture-ram0"]
            geom = s["regions"]["geometry"]
            tile = s["regions"]["tile-ram"]
            print(
                f"{path.name} ip=0x{s['ip']:08x} sel=0x{wu8(w,0x50002A):02x} "
                f"ph=0x{wu8(w,0x500030):02x} cd={wu32(w,0x500024)} "
                f"nz_tex={sum(1 for c in tex[:0x10000] if c not in (0,0xFF))} "
                f"tex={sha16(tex)} geom={sha16(geom)} tiles={tile_strings(tile,60)!r}"
            )
        return


if __name__ == "__main__":
    main()
