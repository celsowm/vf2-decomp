#!/usr/bin/env python3
"""Parse .vf2snap without executing guest code and dump attract state."""
from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

MAGIC = b"VF2SNAP\x00"
REG_COUNT = 18
REG_NAMES = [
    "geometry",
    "copro-port",
    "work-ram",
    "buffer-ram",
    "video-control",
    "cpu-control",
    "interrupt-control",
    "timers",
    "tile-ram",
    "palette-ram",
    "io-control",
    "backup-sram",
    "copro-control",
    "color-translation",
    "texture-ram0",
    "texture-ram1",
    "luma-ram",
    "system-control",
]
# i960: 32 regs? g0-g15 + r0-r15 = 32; local frames 16? * 16 regs?
# From snapshot.c: VF2_I960_REGISTER_COUNT registers then
# VF2_I960_MAX_LOCAL_FRAMES * VF2_I960_LOCAL_REGISTER_COUNT
# Common project values: REG=32, MAX_FRAMES=16, LOCAL=16 → 32 + 256 = 288 u32
# Will detect by region sizes remaining.

WORK_BASE = 0x00500000
BACKUP_BASE = 0x01D00000
WORK_SIZE = 0x00100000
BACKUP_SIZE = 0x00004000


def u32(b: bytes, off: int) -> int:
    return struct.unpack_from("<I", b, off)[0]


def u64(b: bytes, off: int) -> int:
    return struct.unpack_from("<Q", b, off)[0]


def parse_snap(path: Path) -> dict:
    data = path.read_bytes()
    if data[:8] != MAGIC:
        raise ValueError(f"{path}: bad magic {data[:8]!r}")
    off = 8
    version = u32(data, off)
    off += 4
    sat = u32(data, off); off += 4
    prcb = u32(data, off); off += 4
    ip = u32(data, off); off += 4
    pcr = u32(data, off); off += 4
    ac = u32(data, off); off += 4
    ic = u32(data, off); off += 4
    cmp = u32(data, off); off += 4
    reinit = u32(data, off); off += 4
    executed = u64(data, off); off += 8
    calls = u64(data, off); off += 8
    rets = u64(data, off); off += 8
    ient = u64(data, off); off += 8
    iret = u64(data, off); off += 8
    depth = u32(data, off); off += 4
    max_depth = u32(data, off); off += 4
    # Try common register block sizes.
    # After regs come geometry fields then region_count and sizes.
    # Search forward for REG_COUNT=18 as a plausible region_count near expected offset.
    # Default from project headers (read from include if needed).
    # Empirical: parks are 11293256 bytes.
    # We'll try REG=32, FRAMES=32, LOCAL=16 → 32+512=544
    candidates = [
        (32, 128, 16),  # project headers: REGISTER_COUNT=32, MAX_LOCAL_FRAMES=128, LOCAL=16
        (32, 32, 16),
        (32, 16, 16),
    ]
    chosen = None
    for nreg, nframe, nlocal in candidates:
        o = off + (nreg + nframe * nlocal) * 4 + 4 * 4  # regs + 4 geometry u32
        if o + 4 > len(data):
            continue
        rc = u32(data, o)
        if rc != REG_COUNT:
            continue
        sizes = []
        oo = o + 4
        ok = True
        for _ in range(REG_COUNT):
            if oo + 4 > len(data):
                ok = False
                break
            sizes.append(u32(data, oo))
            oo += 4
        if not ok:
            continue
        payload = sum(sizes)
        if oo + payload == len(data) or abs(oo + payload - len(data)) < 16:
            chosen = (nreg, nframe, nlocal, o, sizes, oo)
            break
    if chosen is None:
        raise ValueError(f"{path}: could not locate region table (size={len(data)})")
    nreg, nframe, nlocal, rc_off, sizes, payload_off = chosen
    regs = [u32(data, off + i * 4) for i in range(nreg)]
    gws = u32(data, off + (nreg + nframe * nlocal) * 4)
    grs = u32(data, off + (nreg + nframe * nlocal) * 4 + 4)
    gc = u32(data, off + (nreg + nframe * nlocal) * 4 + 8)
    gpc = u32(data, off + (nreg + nframe * nlocal) * 4 + 12)
    regions = {}
    o = payload_off
    for name, size in zip(REG_NAMES, sizes):
        regions[name] = data[o : o + size]
        o += size
    return {
        "path": path,
        "version": version,
        "ip": ip,
        "executed": executed,
        "calls": calls,
        "rets": rets,
        "cmp": cmp,
        "ac": ac,
        "depth": depth,
        "regs": regs,
        "nreg": nreg,
        "nframe": nframe,
        "nlocal": nlocal,
        "regions": regions,
        "sizes": dict(zip(REG_NAMES, sizes)),
    }


def wu8(work: bytes, addr: int) -> int:
    return work[addr - WORK_BASE]


def wu32(work: bytes, addr: int) -> int:
    return u32(work, addr - WORK_BASE)


def bu8(bk: bytes, addr: int) -> int:
    return bk[addr - BACKUP_BASE]


def bu32(bk: bytes, addr: int) -> int:
    return u32(bk, addr - BACKUP_BASE)


def sha16(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()[:16]


def tile_strings(tile: bytes, limit: int = 180) -> str:
    chars = []
    for i in range(0, min(len(tile), 0x8000) - 1, 2):
        v = tile[i] | (tile[i + 1] << 8)
        if (v & 0xFF00) == 0x8000 and 32 <= (v & 0xFF) < 127:
            chars.append(chr(v & 0xFF))
        else:
            if chars and chars[-1] != "|":
                chars.append("|")
    s = "".join(chars)
    while "||" in s:
        s = s.replace("||", "|")
    return s[:limit]


def dump(path: Path) -> None:
    snap = parse_snap(path)
    work = snap["regions"]["work-ram"]
    tile = snap["regions"]["tile-ram"]
    bk = snap["regions"].get("backup-sram", b"")
    tex0 = snap["regions"].get("texture-ram0", b"")
    geom = snap["regions"].get("geometry", b"")
    print(f"\n== {path.name} ip=0x{snap['ip']:08x} exec={snap['executed']} "
          f"cmp={snap['cmp']} depth={snap['depth']} layout={snap['nreg']}/{snap['nframe']}/{snap['nlocal']}")
    print(
        f"  sel=0x{wu8(work, 0x50002A):02x} phase3=0x{wu8(work, 0x500030):02x} "
        f"a4=0x{wu8(work, 0x5000A4):02x} a5=0x{wu8(work, 0x5000A5):02x} "
        f"a6=0x{wu8(work, 0x5000A6):02x}"
    )
    print(
        f"  cd={wu32(work, 0x500024)} flags68=0x{wu32(work, 0x500068):08x} "
        f"board=0x{wu32(work, 0x508000):08x} ready=0x{wu32(work, 0x550000):08x}"
    )
    print(
        f"  cfgbase=0x{wu32(work, 0x50016C):08x} sig=0x{wu32(work, 0x59CFE0):08x}"
        f"{wu32(work, 0x59CFE4):08x} mode48=0x{wu8(work, 0x500048):02x}"
    )
    print(f"  mask2c=0x{wu32(work, 0x50002C):08x} mask28=0x{wu32(work, 0x500028):08x}")
    cfgbase = wu32(work, 0x50016C)
    if cfgbase >= WORK_BASE:
        print(
            f"  work.country@+3350=0x{wu8(work, cfgbase + 0x3350):02x} "
            f"work.flags@+3351=0x{wu8(work, cfgbase + 0x3351):02x} "
            f"work.coin@+3320={work[cfgbase + 0x3320 - WORK_BASE:cfgbase + 0x3330 - WORK_BASE].hex()}"
        )
    if len(bk) >= 0x3352:
        print(
            f"  backup assign3340={bk[0x3340:0x3352].hex()} "
            f"country=0x{bk[0x3350]:02x} flags3351=0x{bk[0x3351]:02x} "
            f"crc3302=0x{struct.unpack_from('<H', bk, 0x3302)[0]:04x}"
        )
        print(f"  backup coin3320={bk[0x3320:0x3330].hex()}")
    print(f"  tiles: {tile_strings(tile)}")
    print(
        f"  tex0 sha={sha16(tex0)} len={len(tex0)} "
        f"nz64k={sum(1 for b in tex0[:0x10000] if b not in (0, 0xFF))}"
    )
    print(f"  geom sha={sha16(geom)} len={len(geom)}")


def main() -> None:
    args = [Path(a) for a in sys.argv[1:]]
    if not args:
        args = [
            Path("out/park-after-irq.vf2snap"),
            Path("out/sega-natural-frame.vf2snap"),
            Path("out/sega-after-cd.vf2snap"),
            Path("out/sega-sel3-p0.vf2snap"),
            Path("out/park-warm-attract.vf2snap"),
            Path("out/sixth-fresh.vf2snap"),
            Path("out/park-sixth-a6c0.vf2snap"),
            Path("out/sega-rt-sel3.vf2snap"),
            Path("out/sega-natural-sel3.vf2snap"),
            Path("out/sega-rt-sel10.vf2snap"),
            Path("out/sega-sel3-f3.vf2snap"),
            Path("out/park-warm-sel3.vf2snap"),
        ]
    for p in args:
        if not p.exists():
            print(f"missing {p}")
            continue
        try:
            dump(p)
        except Exception as e:
            print(f"FAIL {p}: {e}")


if __name__ == "__main__":
    main()
