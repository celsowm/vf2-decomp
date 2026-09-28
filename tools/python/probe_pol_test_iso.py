#!/usr/bin/env python3
"""Isolated pol_test prim probes via helper 0x7c60 and task 0x21a00 paths.

Evidence only. Memory-trace captures successful Model 2A accesses.
No ROM/snap/trace content for git (parent commits code only).
"""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

PROBE = Path("build/Debug/vf2probe.exe")
ROM = "roms/vf2"
OUT = Path("out/attr-p1")
PARK = Path("out/attr-long/long-29.vf2snap")
IDS = list(range(0x97D, 0x987))  # 0x97d..0x986 inclusive


def run_probe(tag: str, args: list[str]) -> dict:
    OUT.mkdir(parents=True, exist_ok=True)
    jsonl = OUT / f"pol_test_{tag}.jsonl"
    snap = OUT / f"pol_test_{tag}.vf2snap"
    cmd = [str(PROBE), "--rom-dir", ROM, "--snapshot", str(PARK), *args]
    p = subprocess.run(cmd, capture_output=True, text=True, cwd="D:\\ia\\vf2-decomp")
    text = p.stdout + "\n" + p.stderr
    lines = [ln for ln in text.splitlines() if ln.startswith("{")]
    jsonl.write_text("\n".join(lines) + "\n")
    halt = ""
    for ln in reversed(text.splitlines()):
        if '"type":"final"' in ln or '"halt_reason"' in ln:
            try:
                j = json.loads(ln)
                halt = (
                    f"{j.get('halt_reason')} ip={j.get('ip'):#x} "
                    f"run={j.get('run_instructions')}"
                    if isinstance(j.get("ip"), int)
                    else str(j)[-300:]
                )
            except Exception:
                halt = ln[-240:]
            break
    if not lines:
        halt = (halt + " NO_JSON " + text[-400:]).strip()
    return {
        "tag": tag,
        "cmd": cmd,
        "halt": halt,
        "jsonl": str(jsonl),
        "bytes": jsonl.stat().st_size if jsonl.exists() else 0,
        "snap": str(snap),
    }


def main() -> None:
    if not PARK.exists():
        print("NO PARK", file=sys.stderr)
        sys.exit(2)
    if not PROBE.exists():
        print("NO PROBE", file=sys.stderr)
        sys.exit(2)

    results = []

    # A) isolated helper submits 0x7c60 for every pol_test id
    for obj in IDS:
        tag = f"iso_{obj:03x}"
        rec = run_probe(
            tag,
            [
                "--set-ip", "0x00007c60",
                "--until", "0x00007d14",
                "--set-reg", f"g0={obj:#x}",
                "--set-reg", "g1=0",
                "--set-u32", "0x00501018=0x1000",
                "--set-u32", "0x0050101c=0x0",
                "--max-steps", "2500",
                "--memory-trace",
                "--output-snapshot", str(OUT / f"pol_test_{tag}.vf2snap"),
            ],
        )
        print(f"{tag}: {rec['halt'][:120]} bytes={rec['bytes']}")
        results.append(rec)

    # B) task 0x21a00 path A (mode<=2) with count byte=1 → 0x986 then 0x985
    rec = run_probe(
        "task_pathA_986",
        [
            "--set-ip", "0x00021a00",
            "--until", "0x00021b00",
            "--set-u8", "0x00530150=0x2",
            "--set-u8", "0x0053014c=0x1",
            "--set-u32", "0x00501018=0x10000",
            "--set-u32", "0x0050101c=0x0",
            "--max-steps", "8000",
            "--memory-trace",
            "--output-snapshot", str(OUT / "pol_test_task_pathA_986.vf2snap"),
        ],
    )
    print(f"task_pathA_986: {rec['halt'][:140]} bytes={rec['bytes']}")
    results.append(rec)

    # C) task 0x21a00 path A mode==3 → 0x985
    rec = run_probe(
        "task_pathA_985",
        [
            "--set-ip", "0x00021a00",
            "--until", "0x00021b00",
            "--set-u8", "0x00530150=0x3",
            "--set-u8", "0x0053014c=0x1",
            "--set-u32", "0x00501018=0x10000",
            "--set-u32", "0x0050101c=0x0",
            "--max-steps", "8000",
            "--memory-trace",
            "--output-snapshot", str(OUT / "pol_test_task_pathA_985.vf2snap"),
        ],
    )
    print(f"task_pathA_985: {rec['halt'][:140]} bytes={rec['bytes']}")
    results.append(rec)

    # D) task path B mode>2, isolate 0x97f via 0x53014c=1 others 0
    rec = run_probe(
        "task_pathB_97f",
        [
            "--set-ip", "0x00021a00",
            "--until", "0x00021be8",
            "--set-u8", "0x00530150=0x4",
            "--set-u8", "0x0053014c=0x1",
            "--set-u8", "0x0053014d=0x0",
            "--set-u8", "0x0053014e=0x0",
            "--set-u32", "0x00501018=0x10000",
            "--set-u32", "0x0050101c=0x0",
            "--max-steps", "12000",
            "--memory-trace",
            "--output-snapshot", str(OUT / "pol_test_task_pathB_97f.vf2snap"),
        ],
    )
    print(f"task_pathB_97f: {rec['halt'][:140]} bytes={rec['bytes']}")
    results.append(rec)

    # E) task path B isolate 0x97d final call (counts 0) — still does final 0x97d
    rec = run_probe(
        "task_pathB_97d_final",
        [
            "--set-ip", "0x00021a00",
            "--until", "0x00021be8",
            "--set-u8", "0x00530150=0x4",
            "--set-u8", "0x0053014c=0x0",
            "--set-u8", "0x0053014d=0x0",
            "--set-u8", "0x0053014e=0x0",
            "--set-u32", "0x00501018=0x10000",
            "--set-u32", "0x0050101c=0x0",
            "--max-steps", "12000",
            "--memory-trace",
            "--output-snapshot", str(OUT / "pol_test_task_pathB_97d_final.vf2snap"),
        ],
    )
    print(f"task_pathB_97d_final: {rec['halt'][:140]} bytes={rec['bytes']}")
    results.append(rec)

    (OUT / "iso_probe_run.json").write_text(json.dumps(results, indent=2))
    print(f"wrote {OUT / 'iso_probe_run.json'}")


if __name__ == "__main__":
    main()
