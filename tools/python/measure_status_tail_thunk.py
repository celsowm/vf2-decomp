#!/usr/bin/env python3
"""Oracle measure status-tail modes 0x0c/0x0d and attract thunk stream."""
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
OUT = Path("out/attr-v0371")


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


def state(path: Path) -> None:
    if not path.exists():
        print("  missing", path)
        return
    s = parse_snap(path)
    w = s["regions"]["work-ram"]
    t = s["regions"]["tile-ram"]
    x = s["regions"]["texture-ram0"]
    print(
        f"  {path.name} ip=0x{s['ip']:08x} exec={s['executed']} "
        f"sel=0x{wu8(w,0x50002A):02x} b2b=0x{wu8(w,0x50002B):02x} "
        f"ph=0x{wu8(w,0x500030):02x} ready=0x{wu32(w,0x550000):08x} "
        f"tiles={tile_strings(t,60)!r} tex={sha16(x)}"
    )


def tile_window(path: Path, addr: int, length: int = 64) -> None:
    if not path.exists():
        return
    s = parse_snap(path)
    tile = s["regions"]["tile-ram"]
    off = addr - 0x01000000
    if off < 0 or off + length > len(tile):
        print(f"  tile window 0x{addr:08x}: oob")
        return
    chunk = tile[off : off + length]
    chars = []
    for i in range(0, length - 1, 2):
        v = chunk[i] | (chunk[i + 1] << 8)
        hi = v & 0xFF00
        ch = v & 0xFF
        if hi in (0x8000, 0x8800, 0x8900) and 32 <= ch < 127:
            chars.append(chr(ch))
        else:
            chars.append(".")
    print(f"  tile@0x{addr:08x}: {''.join(chars)!r} hex={chunk[:24].hex()}")


def classify_trace(text: str, label: str) -> None:
    writes = Counter()
    tile_w = 0
    rom_reads = Counter()
    for ln in text.splitlines():
        if not ln.startswith("{"):
            continue
        try:
            j = json.loads(ln)
        except Exception:
            continue
        if j.get("type") != "memory":
            continue
        a = j.get("address", 0)
        if j.get("kind") == "write":
            writes[a] += 1
            if 0x01000000 <= a < 0x02000000:
                tile_w += 1
        elif j.get("kind") == "read" and a >= 0x00004d00:
            rom_reads[a] += 1
    print(f"  [{label}] tile_writes~{tile_w} top_writes={[(hex(a), n) for a, n in writes.most_common(8)]}")
    print(f"           top_rom_reads={[(hex(a), n) for a, n in rom_reads.most_common(8)]}")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    src = Path("out/attr-fs/all0-ready1.vf2snap")
    print("SRC")
    state(src)

    cases = [
        ("tail-mode03", ["u8:0x0050002b=3", "0x00508000=0", "0x005502c0=0", "0x005502d0=0",
                          "0x005502e0=0", "u8:0x00550000=1"], "0x0004d25c", "0x0004d2bc", "4000"),
        ("tail-mode0c", ["u8:0x0050002b=12", "0x00508000=0", "0x005502c0=0", "0x005502d0=0",
                          "0x005502e0=0", "u8:0x00550000=1"], "0x0004d25c", "0x0004d2bc", "4000"),
        ("tail-mode0d", ["u8:0x0050002b=13", "0x00508000=0", "0x005502c0=0", "0x005502d0=0",
                          "0x005502e0=0", "u8:0x00550000=1"], "0x0004d25c", "0x0004d2bc", "4000"),
        ("thunk-p14", ["u8:0x00550000=0", "0x00515b50=0"], "0x0000c0a4", "0x00009444", "20"),
        ("thunk-stream", ["u8:0x00550000=0", "0x00515b50=0"], "0x00009444", "0x00009468", "40"),
    ]
    for name, sets, ip, until, steps in cases:
        dst = OUT / f"{name}.vf2snap"
        print(f"\n=== {name} ip={ip} until={until}")
        text = probe(src, dst, sets, ip, until, steps, trace=True)
        (OUT / f"{name}.jsonl").write_text(
            "\n".join(ln for ln in text.splitlines() if ln.startswith("{")) + "\n"
        )
        for ln in reversed(text.splitlines()):
            if '"type":"final"' in ln:
                try:
                    j = json.loads(ln)
                    print(f"  halt={j.get('halt_reason')} ip={j.get('ip')} run={j.get('run_instructions')}")
                except Exception:
                    print(ln[-180:])
                break
        classify_trace(text, name)
        state(dst)
        if "tail" in name:
            tile_window(dst, 0x010000E2)
            tile_window(dst, 0x010040E2)

    # After mode0c, continue common tail from 0x4d29c
    src0c = OUT / "tail-mode0c.vf2snap"
    if src0c.exists():
        dst = OUT / "tail-mode0c-common.vf2snap"
        print("\n=== tail-mode0c continue 0x4d29c→0x4d2bc")
        text = probe(src0c, dst, [], "0x0004d29c", "0x0004d2bc", "4000", trace=True)
        (OUT / "tail-mode0c-common.jsonl").write_text(
            "\n".join(ln for ln in text.splitlines() if ln.startswith("{")) + "\n"
        )
        for ln in reversed(text.splitlines()):
            if '"type":"final"' in ln:
                try:
                    j = json.loads(ln)
                    print(f"  halt={j.get('halt_reason')} ip={j.get('ip')} run={j.get('run_instructions')}")
                except Exception:
                    print(ln[-180:])
                break
        classify_trace(text, "mode0c-common")
        state(dst)
        tile_window(dst, 0x010000E2)
        tile_window(dst, 0x010040E2)

    # Read ROM maincpu strings at status-tail sources
    print("\n=== maincpu ASCII at status-tail sources ===")
    img = bytearray(0x200000)
    ROM_DIR = Path("roms/vf2")
    for name, off in (("epr-18385.12", 0), ("epr-18386.13", 2),
                      ("epr-18387.14", 0x40000), ("epr-18388.15", 0x40002)):
        srcb = (ROM_DIR / name).read_bytes()
        for i in range(0, len(srcb), 2):
            di = off + i * 2
            if di + 1 < len(img):
                img[di] = srcb[i]
                img[di + 1] = srcb[i + 1]
    for addr in (0x4D28C, 0x4D2AC, 0x4D2E8, 0x9444):
        chunk = bytes(img[addr : addr + 32])
        asc = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        print(f"  0x{addr:08x}: {chunk.hex()} ascii={asc!r}")


if __name__ == "__main__":
    main()
