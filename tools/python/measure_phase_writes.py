#!/usr/bin/env python3
"""Who writes phase/mask/ready during attract parks (oracle memory-trace)."""
from __future__ import annotations

import json
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

PROBE = "build/Debug/vf2probe.exe"
ROM = "roms/vf2"
OUT = Path("out/attr-phase-writes")

WATCH = {
    0x00500030: "phase",
    0x00500031: "phase_snap",
    0x00500034: "phase_word",
    0x00500028: "mask28",
    0x0050002A: "sel",
    0x00550000: "ready",
    0x00500024: "cd",
    0x00500068: "flags68",
    0x00500094: "b94",
    0x00515B50: "ctr_task",
}


def probe(src: Path, dst: Path, sets: list[str], ip: str, until: str, steps: str) -> str:
    cmd = [
        PROBE, "--rom-dir", ROM, "--snapshot", str(src),
        "--set-ip", ip, "--until", until, "--max-steps", steps,
        "--output-snapshot", str(dst), "--memory-trace",
    ]
    for s in sets:
        if s.startswith("u8:"):
            cmd += ["--set-u8", s[3:]]
        else:
            cmd += ["--set-u32", s]
    p = subprocess.run(cmd, capture_output=True, text=True)
    return p.stdout + p.stderr


def analyze(text: str, label: str) -> None:
    writes = defaultdict(list)  # addr -> list of (step, bytes, ip_before if any)
    step_ip = {}
    for ln in text.splitlines():
        if not ln.startswith("{"):
            continue
        try:
            j = json.loads(ln)
        except Exception:
            continue
        if j.get("type") == "step":
            step_ip[j.get("step")] = j.get("ip_before")
        if j.get("type") == "memory" and j.get("kind") == "write":
            a = j.get("address", 0)
            if a in WATCH or (0x00500030 <= a <= 0x00500034):
                writes[a].append(
                    (j.get("step"), j.get("bytes", ""), step_ip.get(j.get("step")))
                )
    print(f"\n=== {label} ===")
    if not writes:
        print("  (nenhuma write watchlist)")
        return
    for a in sorted(writes):
        name = WATCH.get(a, f"@{a:08x}")
        recs = writes[a]
        ips = Counter(r[2] for r in recs)
        print(f"  {name} 0x{a:08x} n={len(recs)} top_ip={[(hex(i) if i else None, n) for i, n in ips.most_common(8)]}")
        for rec in recs[:6]:
            print(f"    step={rec[0]} bytes={rec[1]} ip_before={hex(rec[2]) if rec[2] else None}")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    cases = [
        (
            "long-ready0",
            Path("out/attr-fs/all0-ready1.vf2snap"),
            ["u8:0x00550000=0", "u8:0x00500704=0"],
            "0x0000a6c0",
            "0x0004d000",
            "80000",
        ),
        (
            "p14-thunk",
            Path("out/attr-fs/all0-ready1.vf2snap"),
            ["u8:0x00550000=0", "0x00515b50=0"],
            "0x0000c0a4",
            "0x00009444",
            "20",
        ),
        (
            "p14-thunk-body",
            Path("out/attr-fs/all0-ready1.vf2snap"),
            ["u8:0x00550000=0", "0x00515b50=0"],
            "0x00009444",
            "0x0000a010",
            "8000",
        ),
        (
            "p15-mask700-full",
            Path("out/attr-fs/all0-ready1.vf2snap"),
            ["u8:0x00500030=15", "u8:0x00550000=0", "0x00500028=0x00030700"],
            "0x0000c268",
            "0x0000a010",
            "40000",
        ),
        (
            "obj-spin",
            Path("out/attr-tail/long-ready0.vf2snap") if Path("out/attr-tail/long-ready0.vf2snap").exists() else Path("out/attr-fs/all0-ready1.vf2snap"),
            [],
            "0x0004c7cc",
            "0x0000a010",
            "20000",
        ),
    ]
    for name, src, sets, ip, until, steps in cases:
        if not src.exists():
            print("missing src", src)
            continue
        dst = OUT / f"{name}.vf2snap"
        text = probe(src, dst, sets, ip, until, steps)
        (OUT / f"{name}.jsonl").write_text(
            "\n".join(ln for ln in text.splitlines() if ln.startswith("{")) + "\n"
        )
        # halt reason
        for ln in reversed(text.splitlines()):
            if '"type":"final"' in ln:
                try:
                    j = json.loads(ln)
                    print(f"\n[{name}] halt={j.get('halt_reason')} ip={j.get('ip')} run={j.get('run_instructions')}")
                except Exception:
                    print(f"\n[{name}] {ln[-160:]}")
                break
        analyze(text, name)


if __name__ == "__main__":
    main()
