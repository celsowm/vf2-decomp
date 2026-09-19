#!/usr/bin/env python3
"""Oracle: phase15 special mask path (0x700) + memory-trace."""
from __future__ import annotations

import json
import subprocess
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, wu8, wu32, tile_strings, sha16

PROBE = "build/Debug/vf2probe.exe"
ROM = "roms/vf2"
OUT = Path("out/attr-tail")


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


def dump(path: Path) -> None:
    if not path.exists():
        print("  missing", path)
        return
    s = parse_snap(path)
    w = s["regions"]["work-ram"]
    t = s["regions"]["tile-ram"]
    x = s["regions"]["texture-ram0"]
    g = s["regions"]["geometry"]
    print(
        f"  {path.name} ip=0x{s['ip']:08x} exec={s['executed']} "
        f"sel=0x{wu8(w,0x50002A):02x} ph=0x{wu8(w,0x500030):02x} "
        f"ready=0x{wu32(w,0x550000):08x} mask28=0x{wu32(w,0x500028):08x} "
        f"b94=0x{wu8(w,0x500094):02x} board60=0x{wu32(w,0x508060):08x} "
        f"tiles={tile_strings(t,80)!r} geom={sha16(g)} tex={sha16(x)} "
        f"nz={sum(1 for c in x[:0x10000] if c not in (0,0xFF))}"
    )


def classify(text: str, limit: int = 30) -> None:
    writes = Counter()
    samples = []
    tile_writes = 0
    rom_reads = Counter()
    for ln in text.splitlines():
        if not ln.startswith("{"):
            continue
        try:
            j = json.loads(ln)
        except Exception:
            continue
        typ = j.get("type")
        if typ == "memory" and j.get("kind") == "write":
            a = j.get("address", 0)
            writes[a] += 1
            if 0x01000000 <= a < 0x02000000:
                tile_writes += 1
                if len(samples) < limit:
                    samples.append((hex(a), j.get("bytes", "")))
        if typ == "memory" and j.get("kind") == "read":
            a = j.get("address", 0)
            if a >= 0x02000000:
                rom_reads[a] += 1
    print(f"    write_keys={len(writes)} tile_writes~{tile_writes} rom_read_keys={len(rom_reads)}")
    print(f"    top writes: {[(hex(a), n) for a, n in writes.most_common(15)]}")
    print(f"    top main_data reads: {[(hex(a), n) for a, n in rom_reads.most_common(15)]}")
    print(f"    tile samples: {samples[:12]}")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    src = Path("out/attr-fs/all0-ready1.vf2snap")
    print("SRC")
    dump(src)

    # mask28 u32: bytes [mask_lo, mask_hi, sel, x]
    # want mask u16 = 0x0700, sel=3 → 0x??030700; keep high byte 0x03 from park
    cases = [
        (
            "p15-mask700",
            "0x0000c268",
            ["u8:0x00500030=15", "u8:0x00550000=0", "0x00500028=0x00030700"],
            "0x0000a010",
            "20000",
        ),
        (
            "p15-mask380",
            "0x0000c268",
            ["u8:0x00500030=15", "u8:0x00550000=0", "0x00500028=0x00030380"],
            "0x0000a010",
            "20000",
        ),
        (
            "p15-mask1c0",
            "0x0000c268",
            ["u8:0x00500030=15", "u8:0x00550000=0", "0x00500028=0x000301c0"],
            "0x0000a010",
            "20000",
        ),
        (
            "p15-mask540",
            "0x0000c268",
            ["u8:0x00500030=15", "u8:0x00550000=0", "0x00500028=0x00030540"],
            "0x0000a010",
            "20000",
        ),
    ]
    for name, ip, sets, until, steps in cases:
        dst = OUT / f"{name}.vf2snap"
        print(f"\n=== {name}")
        text = probe(src, dst, sets, ip, until, steps, trace=True)
        (OUT / f"{name}.jsonl").write_text(
            "\n".join(ln for ln in text.splitlines() if ln.startswith("{")) + "\n"
        )
        print(text[-200:])
        classify(text)
        dump(dst)

    # also measure 0x54318 with board bit28 clear/unknown
    print("\n=== 0x54318 with mask path already executed (from p15-mask700)")
    src2 = OUT / "p15-mask700.vf2snap"
    if src2.exists():
        dst = OUT / "p15-54318.vf2snap"
        text = probe(src2, dst, [], "0x00054318", "0x00054370", "500", trace=True)
        (OUT / "p15-54318.jsonl").write_text(
            "\n".join(ln for ln in text.splitlines() if ln.startswith("{")) + "\n"
        )
        print(text[-250:])
        classify(text)
        dump(dst)


if __name__ == "__main__":
    main()
