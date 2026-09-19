#!/usr/bin/env python3
"""Measure pol_test helper 0x7c60 live for ids 0x97d..0x985 via vf2probe.

Streams memory-trace JSONL to out/attr-p1/pol_test_*.jsonl.
Dumps writes to geo/FIFO ports (0x800010, 0x804000+, 0x884000+).
"""
from __future__ import annotations

import json
import subprocess
import sys
from collections import Counter
from pathlib import Path

PROBE = Path("build/Debug/vf2probe.exe")
ROM = "roms/vf2"
OUT = Path("out/attr-p1")
# Parks with measured attract/pol_test context
PARKS = [
    Path("out/attr-long/long-29.vf2snap"),
    Path("out/sixth-fresh.vf2snap"),
    Path("out/attr-v0372/pol7c60-id985.vf2snap"),
]
IDS = list(range(0x97D, 0x986))  # 0x97d..0x985


def classify_writes(jsonl_path: Path) -> dict:
    writes = Counter()
    geo = []
    fifo = []
    proto = []
    table_reads = []
    for ln in jsonl_path.read_text(errors="replace").splitlines():
        if not ln.startswith("{"):
            continue
        try:
            j = json.loads(ln)
        except Exception:
            continue
        if j.get("type") != "memory":
            continue
        a = int(j.get("address", 0))
        kind = j.get("kind")
        try:
            bs = bytes.fromhex(j.get("bytes", "00"))
            val = int.from_bytes(bs[:4], "little")
        except Exception:
            val = 0
            bs = b""
        if kind == "write":
            writes[a] += 1
            if a == 0x00800010:
                geo.append(("0x800010", val, j.get("step")))
            elif 0x00804000 <= a < 0x00808000:
                geo.append((f"{a:#x}", val, j.get("step")))
            elif a in (0x00884000, 0x00880000) or (
                0x00884000 <= a < 0x00888000
            ):
                fifo.append((f"{a:#x}", val, j.get("step")))
            if val in (0x00800101, 0x01800303, 0x03000606, 0x1A003434) or (
                (val & 0x00FFFFFF) in (0x800101, 0x800303, 0x606, 0x3434)
            ):
                proto.append((f"{a:#x}", f"{val:#010x}", j.get("step")))
        elif kind == "read":
            if 0x020E0000 <= a < 0x02100000:
                table_reads.append((f"{a:#x}", val, j.get("step")))
            if a in (0x00501018, 0x0050101C, 0x1A003434):
                table_reads.append((f"gate/{a:#x}", val, j.get("step")))
    return {
        "top_writes": [(hex(a), n) for a, n in writes.most_common(20)],
        "geo_writes": [(a, f"{v:#010x}", s) for a, v, s in geo[:80]],
        "fifo_writes": [(a, f"{v:#010x}", s) for a, v, s in fifo[:80]],
        "protocol_samples": proto[:40],
        "table_reads": [(a, f"{v:#010x}", s) for a, v, s in table_reads[:40]],
        "counts": {
            "geo": len(geo),
            "fifo": len(fifo),
            "proto": len(proto),
            "table_reads": len(table_reads),
        },
    }


def run_one(park: Path, obj_id: int, tag: str) -> dict | None:
    dst = OUT / f"pol_test_{tag}_{obj_id:03x}.vf2snap"
    jsonl = OUT / f"pol_test_{tag}_{obj_id:03x}.jsonl"
    # Gate open: 0x50101c > 0x501018 would early-ret; keep 0x1000 vs 0.
    cmd = [
        str(PROBE),
        "--rom-dir", ROM,
        "--snapshot", str(park),
        "--set-ip", "0x00007c60",
        "--until", "0x00007d14",
        "--set-reg", f"g0={obj_id:#x}",
        "--set-u32", "0x00501018=0x1000",
        "--set-u32", "0x0050101c=0x0",
        "--max-steps", "4000",
        "--memory-trace",
        "--output-snapshot", str(dst),
    ]
    p = subprocess.run(cmd, capture_output=True, text=True, cwd="D:\\ia\\vf2-decomp")
    text = p.stdout + "\n" + p.stderr
    lines = [ln for ln in text.splitlines() if ln.startswith("{")]
    jsonl.write_text("\n".join(lines) + "\n")
    halt = ""
    for ln in reversed(text.splitlines()):
        if '"type":"final"' in ln:
            try:
                j = json.loads(ln)
                halt = f"{j.get('halt_reason')} ip={j.get('ip'):#x} run={j.get('run_instructions')}"
            except Exception:
                halt = ln[-200:]
            break
    summary = classify_writes(jsonl)
    summary["id"] = obj_id
    summary["id_hex"] = f"{obj_id:#x}"
    summary["park"] = str(park)
    summary["halt"] = halt
    summary["jsonl"] = str(jsonl)
    summary["jsonl_bytes"] = jsonl.stat().st_size if jsonl.exists() else 0
    # stdout snippet for errors
    if not lines:
        summary["probe_error"] = text[-500:]
    return summary


def analyze_existing(path: Path) -> dict:
    return classify_writes(path)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    park = next((p for p in PARKS if p.exists()), None)
    if park is None:
        print("NO PARK", file=sys.stderr)
        sys.exit(2)

    # Re-analyze existing v0372 traces
    existing = {}
    for p in sorted(Path("out/attr-v0372").glob("pol7c60-*.jsonl")):
        existing[p.name] = analyze_existing(p)
        print(
            f"existing {p.name}: geo={existing[p.name]['counts']['geo']} "
            f"fifo={existing[p.name]['counts']['fifo']} "
            f"proto={existing[p.name]['counts']['proto']}"
        )
        print(f"  geo sample={existing[p.name]['geo_writes'][:8]}")
        print(f"  fifo sample={existing[p.name]['fifo_writes'][:8]}")

    live = []
    for obj_id in IDS:
        print(f"\n=== probe pol_test id={obj_id:#x} park={park}")
        rec = run_one(park, obj_id, "g7c60")
        if rec:
            live.append(rec)
            print(f"  halt={rec.get('halt')} jsonl={rec.get('jsonl_bytes')}B")
            print(f"  top_writes={rec.get('top_writes')[:10]}")
            print(f"  geo={rec.get('geo_writes')[:12]}")
            print(f"  fifo={rec.get('fifo_writes')[:12]}")
            print(f"  proto={rec.get('protocol_samples')[:8]}")
            print(f"  table_reads={rec.get('table_reads')[:8]}")
            if rec.get("probe_error"):
                print(f"  ERR={rec['probe_error'][-200:]}")

    report = {
        "park": str(park),
        "helper": "0x7c60",
        "gate": "0x501018=0x1000 0x50101c=0 (open: 0x50101c > 0x501018 is false)",
        "ids": [f"{i:#x}" for i in IDS],
        "existing_v0372": existing,
        "live": live,
        "note": (
            "Memory events are successful Model 2A accesses. Correlate by step; "
            "do not infer IP from already-advanced address."
        ),
    }
    (OUT / "pol_test_probe_summary.json").write_text(json.dumps(report, indent=2))
    print(f"\nwrote {OUT / 'pol_test_probe_summary.json'}")


if __name__ == "__main__":
    main()
