#!/usr/bin/env python3
"""v0372B: boot-park selector FIFO sweep + tile residue dump."""
from __future__ import annotations

import json
import subprocess
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, wu8, wu32

PROBE = "build/Debug/vf2probe.exe"
ROM = "roms/vf2"
OUT = Path("out/attr-v0372")
FIFO = 0x00884000


def run(src, dst, sets, ip, until, steps) -> str:
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


def classify(text: str) -> dict:
    fifo = 0
    funcs = Counter()
    vals = Counter()
    geo_w = 0
    tile_w = 0
    upload = 0
    funcp = 0
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
        bs = bytes.fromhex(j.get("bytes") or "00")
        val = int.from_bytes(bs, "little") if bs else 0
        if a == FIFO:
            fifo += 1
            funcs[(val >> 23) & 0x3F] += 1
            vals[val] += 1
        elif a in (FIFO + 4, FIFO + 8, FIFO + 12):
            fifo += 1
        elif 0x00800000 <= a < 0x00808000:
            geo_w += 1
        elif 0x01000000 <= a < 0x02000000:
            tile_w += 1
        elif 0x00980000 <= a < 0x00982000:
            upload += 1
        elif a == 0x00880000:
            funcp += 1
    return {
        "fifo": fifo,
        "funcs": sorted(funcs.items(), key=lambda x: -x[1])[:10],
        "top_vals": [(f"0x{v:08x}", n) for v, n in vals.most_common(6)],
        "geo_w": geo_w,
        "tile_w": tile_w,
        "upload": upload,
        "funcp": funcp,
    }


def tile_at(path: Path, start: int, length: int = 64) -> str:
    if not path.exists():
        return "missing"
    s = parse_snap(path)
    t = s["regions"]["tile-ram"]
    off = start - 0x01000000
    if off < 0:
        off = start if start < len(t) else 0
    chunk = t[off : off + length]
    chars = []
    for i in range(0, length - 1, 2):
        v = chunk[i] | (chunk[i + 1] << 8)
        hi = v & 0xFF00
        ch = v & 0xFF
        if hi in (0x8000, 0x8800, 0x8900) and 32 <= ch < 127:
            chars.append(chr(ch))
        else:
            chars.append(".")
    return f"{start:#x}:{chunk[:32].hex()} {''.join(chars)}"


def main() -> None:
    boot = Path("out/park-after-irq.vf2snap")
    all0 = Path("out/attr-fs/all0-ready1.vf2snap")
    results = []

    # Decode t4e jef residue from sel09-r0-a6c0 across tile plane
    sn = OUT / "sel09-r0-a6c0.vf2snap"
    if sn.exists():
        print("=== tile residue windows sel09-r0-a6c0 ===")
        s = parse_snap(sn)
        t = s["regions"]["tile-ram"]
        # scan for non-space glyphs
        for off in range(0, min(len(t), 0x8000) - 1, 2):
            v = t[off] | (t[off + 1] << 8)
            if v not in (0, 0x20, 0x8020, 0x2000, 0x0020) and (v & 0xFF00) in (
                0x8000,
                0x8800,
                0x8900,
            ):
                addr = 0x01000000 + off
                print(f"  glyph@0x{addr:08x} = 0x{v:04x}")
                if off < 0x2000:
                    print("   ", tile_at(sn, addr, 48))
                if off > 0x400:
                    break

    # Boot-park selector sweep focused on FIFO
    print("\n=== boot-park selector FIFO sweep (40k steps) ===")
    for sel in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 15, 16, 17, 18, 19):
        name = f"boot-sel{sel:02d}-fifo"
        dst = OUT / f"{name}.vf2snap"
        sets = [
            f"u8:0x0050002a={sel}",
            f"u8:0x0050002b={sel}",
            f"0x00500028=0x{(sel << 24) | (sel << 16):08x}",
            "u8:0x00550000=0",
            "u8:0x00500704=0",
        ]
        text = run(boot, dst, sets, "0x0000a6c0", "0x0004d000", "40000")
        (OUT / f"{name}.jsonl").write_text(
            "\n".join(ln for ln in text.splitlines() if ln.startswith("{")) + "\n"
        )
        c = classify(text)
        st = {}
        if dst.exists():
            s = parse_snap(dst)
            w = s["regions"]["work-ram"]
            st = {
                "sel": wu8(w, 0x50002A),
                "ph": wu8(w, 0x500030),
                "ready": wu32(w, 0x550000),
            }
        rec = {"name": name, "class": c, "state": st}
        results.append(rec)
        print(f"  {name}: {st} {c}")

    # Longer sel9 from boot
    print("\n=== boot-sel09 long 120k ===")
    dst = OUT / "boot-sel09-long.vf2snap"
    text = run(
        boot,
        dst,
        [
            "u8:0x0050002a=9",
            "u8:0x0050002b=9",
            "0x00500028=0x09090000",
            "u8:0x00550000=0",
            "u8:0x00500704=0",
        ],
        "0x0000a6c0",
        "0x0004d000",
        "120000",
    )
    (OUT / "boot-sel09-long.jsonl").write_text(
        "\n".join(ln for ln in text.splitlines() if ln.startswith("{")) + "\n"
    )
    c = classify(text)
    st = {}
    if dst.exists():
        s = parse_snap(dst)
        w = s["regions"]["work-ram"]
        t = s["regions"]["tile-ram"]
        st = {
            "sel": wu8(w, 0x50002A),
            "ph": wu8(w, 0x500030),
            "ready": wu32(w, 0x550000),
            "tiles_head": tile_at(dst, 0x01000000, 80),
        }
    print("  long:", st, c)
    results.append({"name": "boot-sel09-long", "class": c, "state": st})

    (OUT / "boot_fifo_sweep.json").write_text(json.dumps(results, indent=2))
    print("\nWrote", OUT / "boot_fifo_sweep.json")


if __name__ == "__main__":
    main()
