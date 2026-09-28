#!/usr/bin/env python3
"""Dump TGP object table at main_data 0x020e0004 + measure fa_pol_test oracle."""
from __future__ import annotations

import json
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, wu8, wu32, tile_strings, sha16

ROM_DIR = Path("roms/vf2")
PROBE = "build/Debug/vf2probe.exe"
ROM = "roms/vf2"
OUT = Path("out/attr-logo")
BASE = 0x02000000
TABLE = 0x020E0004

PAIRS = [
    ("mpr-17560.10", 0x00000000),
    ("mpr-17561.11", 0x00000002),
    ("mpr-17558.8", 0x00400000),
    ("mpr-17559.9", 0x00400002),
    ("mpr-17566.6", 0x00800000),
    ("mpr-17567.7", 0x00800002),
    ("mpr-17564.4", 0x00C00000),
    ("mpr-17565.5", 0x00C00002),
]


def build_main_data() -> bytes:
    region = bytearray(0x02400000)
    for name, off in PAIRS:
        p = ROM_DIR / name
        if not p.exists():
            continue
        src = p.read_bytes()
        for i in range(0, len(src), 2):
            dest = off + (i // 2) * 4
            if dest + 1 < len(region):
                region[dest] = src[i]
                region[dest + 1] = src[i + 1]
    return bytes(region)


def dump_table(img: bytes, ids: list[int]) -> None:
    print(f"=== object table @ 0x{TABLE:08x} (main_data +0x{TABLE-BASE:x}) ===")
    for obj in ids:
        off = (TABLE - BASE) + obj * 16
        if off + 16 > len(img):
            print(f"  id=0x{obj:03x} OOB")
            continue
        rec = img[off : off + 16]
        words = [struct.unpack_from("<I", rec, i)[0] for i in range(0, 16, 4)]
        print(f"  id=0x{obj:03x} off=0x{off:x} words={[hex(w) for w in words]} hex={rec.hex()}")


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
    g = s["regions"].get("geometry", b"")
    print(
        f"  {path.name} ip=0x{s['ip']:08x} exec={s['executed']} "
        f"sel=0x{wu8(w,0x50002A):02x} ph=0x{wu8(w,0x500030):02x} "
        f"g11slot=0x{wu32(w,0x501010):08x}/0x{wu32(w,0x501018):08x}/0x{wu32(w,0x50101C):08x} "
        f"53014c={wu8(w,0x53014C):02x}{wu8(w,0x53014D):02x}{wu8(w,0x53014E):02x}"
        f"{wu8(w,0x53014F):02x} 530150={wu8(w,0x530150):02x} "
        f"tiles={tile_strings(t,50)!r} nz={sum(1 for c in x[:0x10000] if c not in (0,0xFF))} "
        f"geom={sha16(g)}"
    )


def classify(text: str) -> None:
    from collections import Counter
    writes = Counter()
    reads = Counter()
    fifo = []
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
            if a in (0x00884000, 0x00880000, 0x00980000) or a >= 0x00800000 and a < 0x00900000:
                try:
                    bs = bytes.fromhex(j.get("bytes", "00"))
                    fifo.append((hex(a), bs[:4].hex(), hex(int.from_bytes(bs[:4], "little"))))
                except Exception:
                    pass
        elif j.get("kind") == "read" and (0x020E0000 <= a < 0x020E2000 or a in (0x00884000, 0x00501018, 0x0050101C)):
            reads[a] += 1
    print(f"  top writes={[(hex(a), n) for a, n in writes.most_common(12)]}")
    print(f"  table/geo reads={[(hex(a), n) for a, n in reads.most_common(12)]}")
    print(f"  fifo/geo samples ({len(fifo)}): {fifo[:20]}")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    img = build_main_data()
    ids = list(range(0x970, 0x990)) + [0x985, 0x986, 0x97f, 0x97e, 0x97d]
    dump_table(img, sorted(set(ids)))
    # also dump 0x70cbc table mentioned in notes
    for addr in (0x00070CBC, 0x020E0004, 0x020E0985 * 1):
        off = addr - BASE if addr >= BASE else addr
        if 0 <= off + 32 <= len(img):
            words = [struct.unpack_from("<I", img, off + i)[0] for i in range(0, 32, 4)]
            print(f"  raw @cpu/file 0x{addr:08x} off=0x{off:x} {[hex(w) for w in words]}")

    src_candidates = [
        Path("out/sixth-fresh.vf2snap"),
        Path("out/attr-fs/all0-ready1.vf2snap"),
        Path("out/park-sixth-a6c0.vf2snap"),
    ]
    src = next((p for p in src_candidates if p.exists()), None)
    if src is None:
        print("no src park")
        return
    print(f"\nSRC {src}")
    state(src)

    # Force fa_pol_test: entry 0x21a00, state 0x500874
    # Need runnable flags on the task slot + g11/g12 FIFO cursors
    # Measured: g11 often 0x884000 via runtime; cursor g12 at work.
    # Arm slot: typical pattern flags bit31 + entry at state ptr.
    # From tasks.csv state_address=0x500874 for fa_pol_test.
    cases = [
        (
            "pol-direct",
            ["u8:0x00530150=0", "u8:0x0053014c=0"],
            "0x00021a00",
            "0x000010dcc",
            "20000",
        ),
        (
            "pol-state2",
            ["u8:0x00530150=2", "u8:0x0053014c=1"],
            "0x00021a00",
            "0x000010dcc",
            "20000",
        ),
        (
            "pol-7c60-id985",
            ["u8:0x00530150=0"],
            "0x00007c60",
            "0x00007d14",
            "2000",
        ),
    ]
    for name, sets, ip, until, steps in cases:
        dst = OUT / f"{name}.vf2snap"
        print(f"\n=== {name} ip={ip}")
        # For 7c60 need g0=0x985 — probe may not set regs; still measure.
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
        classify(text)
        state(dst)


if __name__ == "__main__":
    main()
