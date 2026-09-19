#!/usr/bin/env python3
"""Map ROM stib sites to 0x00500030 and measure 0x4d25c with board bit9 clear."""
from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
# avoid dump_sel0 side-effect import
import importlib.util

spec = importlib.util.spec_from_file_location(
    "dumps0", Path("tools/python/dump_sel0_strings.py")
)

PROBE = "build/Debug/vf2probe.exe"
I960 = "build/Debug/vf2i960.exe"
ROM = "roms/vf2"


def build_maincpu() -> bytes:
    ROM_DIR = Path("roms/vf2")
    region = bytearray(0x200000)
    pairs = (
        ("epr-18385.12", 0x00000000),
        ("epr-18386.13", 0x00000002),
        ("epr-18387.14", 0x00040000),
        ("epr-18388.15", 0x00040002),
    )
    for name, off in pairs:
        src = (ROM_DIR / name).read_bytes()
        for i in range(0, len(src), 2):
            di = off + i * 2
            if di + 1 < len(region):
                region[di] = src[i]
                region[di + 1] = src[i + 1]
    return bytes(region)


def stib_phase_sites(img: bytes) -> list[int]:
    """stib/ldib absolute phase: word c0783000/c2783000 then addr 0x00500030."""
    sites = []
    needle = struct.pack("<I", 0x00500030)
    stib = struct.pack("<I", 0xC2783000)
    ldib = struct.pack("<I", 0xC0783000)
    start = 0
    while True:
        i = img.find(needle, start)
        if i < 0:
            break
        if i >= 4:
            prev = img[i - 4 : i]
            if prev == stib or prev == ldib:
                sites.append((i - 4, "stib" if prev == stib else "ldib"))
        start = i + 1
    return sites


def disasm(addr: int, n: int = 12) -> str:
    p = subprocess.run(
        [I960, "disasm", ROM, hex(addr), str(n)],
        capture_output=True, text=True,
    )
    return p.stdout + p.stderr


def probe(src, dst, sets, ip, until, steps):
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
    p = subprocess.run(cmd, capture_output=True, text=True)
    return p.stdout + p.stderr


def state(path: Path) -> None:
    sys.path.insert(0, str(Path("tools/python").resolve()))
    from dump_attract_state import parse_snap, wu8, wu32
    if not path.exists():
        print("  missing", path)
        return
    s = parse_snap(path)
    w = s["regions"]["work-ram"]
    print(
        f"  {path.name} ip=0x{s['ip']:08x} exec={s['executed']} "
        f"sel=0x{wu8(w,0x50002A):02x} ph=0x{wu8(w,0x500030):02x} "
        f"ready=0x{wu32(w,0x550000):08x} board=0x{wu32(w,0x508000):08x} "
        f"status=0x{wu32(w,0x55C2F0):08x} ctr0=0x{wu32(w,0x5502C0):08x} "
        f"ctr2=0x{wu32(w,0x5502E0):08x}"
    )


def main() -> None:
    img = build_maincpu()
    sites = stib_phase_sites(img)
    print(f"=== phase 0x500030 ldib/stib sites in maincpu ({len(sites)}) ===")
    for addr, kind in sites:
        print(f"  {kind} @ 0x{addr:08x}")

    out = Path("out/attr-phase-writes")
    out.mkdir(parents=True, exist_ok=True)
    src = Path("out/attr-fs/all0-ready1.vf2snap")

    print("\n=== final-status 0x4bf90 board bit9 CLEAR ===")
    dst = out / "fs-bit9clear.vf2snap"
    print(probe(
        src, dst,
        ["0x00508000=0", "0x005502c0=0", "0x005502d0=0", "0x005502e0=0", "u8:0x00550000=1"],
        "0x0004bf90", "0x0004bfdc", "200",
    )[-400:])
    state(dst)

    print("\n=== final-status board bit9 clear, until 0x4d25c ===")
    dst2 = out / "fs-4d25c.vf2snap"
    print(probe(
        src, dst2,
        ["0x00508000=0", "0x005502c0=0", "0x005502d0=0", "0x005502e0=0", "u8:0x00550000=1"],
        "0x0004bf90", "0x0004d25c", "200",
    )[-400:])
    state(dst2)

    print("\n=== run 0x4d25c body from all0-ready1 ===")
    dst3 = out / "body-4d25c.vf2snap"
    print(probe(
        src, dst3,
        ["0x00508000=0", "u8:0x00550000=1"],
        "0x0004d25c", "0x0004d400", "4000",
    )[-400:])
    state(dst3)

    print("\n=== disasm key store sites ===")
    for addr, kind in sites:
        if addr < 0xC000 or addr in (0xAC80, 0xACF8, 0xAD08, 0xC3B0, 0xC430, 0xC448):
            print(f"\n--- {kind} 0x{addr:08x} ---")
            print(disasm(addr, 10)[:600])


if __name__ == "__main__":
    main()
