#!/usr/bin/env python3
"""Sweep attract config mutations from a post-SEGA park via vf2probe."""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, wu8, wu32, tile_strings  # noqa: E402

PROBE = Path("build/Debug/vf2probe.exe")
ROM = Path("roms/vf2")
CFG = 0x00599000
COUNTRY = CFG + 0x3350
FLAGS = CFG + 0x3351
COIN = CFG + 0x3320


def run(base: Path, out: Path, sets: list[str], max_steps: str = "1500000") -> dict:
    cmd = [
        str(PROBE),
        "--rom-dir",
        str(ROM),
        "--snapshot",
        str(base),
        "--until",
        "0x0000a010",
        "--max-steps",
        max_steps,
        "--output-snapshot",
        str(out),
    ]
    for s in sets:
        cmd.extend(["--set-u8", s])
    p = subprocess.run(cmd, capture_output=True, text=True)
    raw = p.stdout + p.stderr
    rec = {"base": base.name, "sets": sets, "out": out.name, "rc": p.returncode}
    try:
        # last JSON object
        line = [ln for ln in raw.splitlines() if ln.startswith("{")][-1]
        j = json.loads(line)
        rec["run_instructions"] = j.get("run_instructions")
        rec["halt_reason"] = j.get("halt_reason")
        rec["ip"] = j.get("ip")
        rec["status"] = j.get("status")
    except Exception:
        rec["raw"] = raw[-800:]
    if out.exists():
        snap = parse_snap(out)
        work = snap["regions"]["work-ram"]
        rec["sel"] = wu8(work, 0x50002A)
        rec["phase"] = wu8(work, 0x500030)
        rec["a4"] = wu8(work, 0x5000A4)
        rec["a6"] = wu8(work, 0x5000A6)
        rec["cd"] = wu32(work, 0x500024)
        rec["flags"] = wu32(work, 0x500068)
        rec["board"] = wu32(work, 0x508000)
        rec["country"] = wu8(work, COUNTRY)
        rec["assign"] = wu8(work, FLAGS)
        rec["tiles"] = tile_strings(snap["regions"]["tile-ram"], 80)
        rec["is_test"] = rec["sel"] == 0x11 and rec["a4"] == 0x0B
    return rec


def main() -> None:
    base = Path(sys.argv[1] if len(sys.argv) > 1 else "out/sega-sel3-p0.vf2snap")
    outdir = Path("out/attr-sweep")
    outdir.mkdir(parents=True, exist_ok=True)
    cases: list[tuple[str, list[str]]] = [
        ("baseline", []),
        ("country1", [f"{COUNTRY:#x}=1"]),
        ("country2", [f"{COUNTRY:#x}=2"]),
        ("flag0", [f"{FLAGS:#x}=1"]),
        ("flag1", [f"{FLAGS:#x}=2"]),
        ("flag2", [f"{FLAGS:#x}=4"]),
        ("flag3", [f"{FLAGS:#x}=8"]),
        ("flag5", [f"{FLAGS:#x}=0x20"]),
        ("flag6", [f"{FLAGS:#x}=0x40"]),
        ("coin0", [f"{COIN:#x}=1"]),
        ("c1_flag0", [f"{COUNTRY:#x}=1", f"{FLAGS:#x}=1"]),
        ("c1_coin0", [f"{COUNTRY:#x}=1", f"{COIN:#x}=1"]),
        ("c2_flag0", [f"{COUNTRY:#x}=2", f"{FLAGS:#x}=1"]),
    ]
    results = []
    for name, sets in cases:
        out = outdir / f"{base.stem}-{name}.vf2snap"
        rec = run(base, out, sets)
        results.append(rec)
        print(
            f"{name:12} sets={sets} ip=0x{rec.get('ip',0):08x} "
            f"insns={rec.get('run_instructions')} halt={rec.get('halt_reason')} "
            f"sel=0x{rec.get('sel',0):02x} ph=0x{rec.get('phase',0):02x} "
            f"a4=0x{rec.get('a4',0):02x} a6=0x{rec.get('a6',0):02x} "
            f"cd={rec.get('cd')} board=0x{rec.get('board',0):08x} "
            f"TEST={rec.get('is_test')} tiles={rec.get('tiles','')[:60]!r}"
        )
    Path(outdir / f"{base.stem}-results.json").write_text(
        json.dumps(results, indent=2), encoding="utf-8"
    )
    print(f"wrote {outdir}/{base.stem}-results.json")


if __name__ == "__main__":
    main()
