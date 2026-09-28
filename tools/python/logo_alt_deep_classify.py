#!/usr/bin/env python3
"""v0372 deep classify: FIFO/geo packets + tile decode from campaign parks."""
from __future__ import annotations

import json
import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, wu8, wu32, sha16

OUT = Path("out/attr-v0372")
FIFO = 0x00884000
GEO_LO = 0x00800000
GEO_HI = 0x00808000


def decode_all_banks(tile: bytes, limit=200) -> str:
    chars = []
    for i in range(0, min(len(tile), 0x4000) - 1, 2):
        v = tile[i] | (tile[i + 1] << 8)
        hi = v & 0xFF00
        ch = v & 0xFF
        if hi in (0x8000, 0x8800, 0x8900) and 32 <= ch < 127:
            chars.append(chr(ch))
        elif hi in (0x8000, 0x8800, 0x8900):
            chars.append(f"[{v:04x}]")
        else:
            if chars and chars[-1] != "|":
                chars.append("|")
    s = "".join(chars)
    while "||" in s:
        s = s.replace("||", "|")
    return s[:limit]


def dump_tile_window(path: Path, addr=0x01000000, length=128) -> None:
    s = parse_snap(path)
    tile = s["regions"]["tile-ram"]
    off = addr - 0x01000000
    chunk = tile[off : off + length]
    print(f"  tile@0x{addr:08x} hex={chunk[:64].hex()}")
    print(f"  banks: {decode_all_banks(tile, 180)!r}")


def classify_jsonl(path: Path) -> dict:
    fifo_words = []
    fifo_func = Counter()
    fifo_pairs = []  # (func, value)
    geo_w = Counter()
    geo_r = Counter()
    func_port = Counter()
    upload = Counter()
    main_data_reads = Counter()
    rom_model_reads = Counter()
    for ln in path.read_text(errors="replace").splitlines():
        if not ln.startswith("{"):
            continue
        try:
            j = json.loads(ln)
        except Exception:
            continue
        if j.get("type") != "memory":
            continue
        a = j.get("address", 0)
        bs = bytes.fromhex(j.get("bytes") or "00")
        val = int.from_bytes(bs, "little") if bs else 0
        kind = j.get("kind")
        if kind == "write":
            if a == FIFO:
                fifo_words.append(val)
                fn = (val >> 23) & 0x3F
                fifo_func[fn] += 1
                fifo_pairs.append((fn, val))
            elif a in (FIFO + 4, FIFO + 8):
                fifo_words.append(val)
            elif GEO_LO <= a < GEO_HI:
                geo_w[a] += 1
            elif a == 0x00880000:
                func_port[val] += 1
            elif 0x00980000 <= a < 0x00982000:
                upload[val] += 1
        elif kind == "read":
            if GEO_LO <= a < GEO_HI:
                geo_r[a] += 1
            if 0x02000000 <= a < 0x04000000:
                main_data_reads[val] += 1
            if 0x02A60000 <= a <= 0x02A80000:
                rom_model_reads[a] += 1
    # unique FIFO values by function code
    by_func = {}
    for fn, val in fifo_pairs:
        by_func.setdefault(fn, Counter())[val] += 1
    return {
        "fifo_n": len(fifo_words),
        "fifo_func_hist": sorted(fifo_func.items(), key=lambda x: -x[1]),
        "fifo_by_func": {
            fn: [(f"0x{v:08x}", n) for v, n in c.most_common(6)]
            for fn, c in sorted(by_func.items(), key=lambda x: -sum(x[1].values()))
        },
        "fifo_unique": len(set(fifo_words)),
        "geo_w": [(f"0x{a:08x}", n) for a, n in geo_w.most_common(12)],
        "geo_r": [(f"0x{a:08x}", n) for a, n in geo_r.most_common(12)],
        "func_port": [(f"0x{v:08x}", n) for v, n in func_port.most_common(8)],
        "upload": [(f"0x{v:08x}", n) for v, n in upload.most_common(8)],
        "maindata_top": [(f"0x{v:08x}", n) for v, n in main_data_reads.most_common(10)],
        "rom_model_addrs": [(f"0x{a:08x}", n) for a, n in rom_model_reads.most_common(10)],
    }


def snap_regions(path: Path) -> dict:
    s = parse_snap(path)
    w = s["regions"]["work-ram"]
    g = s["regions"]["geometry"]
    b = s["regions"]["buffer-ram"]
    x = s["regions"]["texture-ram0"]
    t = s["regions"]["tile-ram"]
    # geometry snapshot first non-zero dwords
    g_nz = []
    for i in range(0, min(len(g), 0x8000), 4):
        v = int.from_bytes(g[i : i + 4], "little")
        if v:
            g_nz.append((0x00800000 + i, v))
        if len(g_nz) >= 16:
            break
    return {
        "file": path.name,
        "sel": wu8(w, 0x50002A),
        "mode2b": wu8(w, 0x50002B),
        "ph": wu8(w, 0x500030),
        "ready": wu32(w, 0x550000),
        "mask28": f"0x{wu32(w, 0x500028):08x}",
        "geom_sha": sha16(g),
        "geom_nz_dwords": sum(
            1
            for i in range(0, min(len(g), 0x8000), 4)
            if int.from_bytes(g[i : i + 4], "little")
        ),
        "geom_head_nz": [(f"0x{a:08x}", f"0x{v:08x}") for a, v in g_nz],
        "buf_sha": sha16(b),
        "tex_sha": sha16(x),
        "nz_tex64k": sum(1 for c in x[:0x10000] if c not in (0, 0xFF)),
        "tile_sha": sha16(t),
    }


def main() -> None:
    interesting = [
        "boot-sel09-force",
        "sel09-ph00-cfbc",
        "sel09-r0-a6c0",
        "sel00-r0-a6c0",
        "sel06-r0-a6c0",
        "sel15-r0-a6c0",
        "sel03-r0-mode03",
        "boot-sel03-force",
    ]
    print("=== JSONL deep classify ===")
    for name in interesting:
        jl = OUT / f"{name}.jsonl"
        sn = OUT / f"{name}.vf2snap"
        print(f"\n-- {name}")
        if jl.exists():
            print(json.dumps(classify_jsonl(jl), indent=2)[:2400])
        else:
            print("  no jsonl")
        if sn.exists():
            print(json.dumps(snap_regions(sn), indent=2))
            dump_tile_window(sn)
        else:
            print("  no snap")

    # Compare geom deltas vs all0-ready1 baseline
    print("\n=== geom/tex delta vs all0-ready1 ===")
    base = Path("out/attr-fs/all0-ready1.vf2snap")
    if base.exists():
        bs = parse_snap(base)
        bg = bs["regions"]["geometry"]
        bx = bs["regions"]["texture-ram0"]
        for name in interesting:
            sn = OUT / f"{name}.vf2snap"
            if not sn.exists():
                continue
            s = parse_snap(sn)
            g = s["regions"]["geometry"]
            x = s["regions"]["texture-ram0"]
            dg = sum(1 for a, b in zip(bg, g) if a != b)
            dx = sum(1 for a, b in zip(bx, x) if a != b)
            print(f"  {name}: geom_bytes_diff={dg} tex_bytes_diff={dx}")

    # ROM cross-ref: FIFO words vs known descriptor addrs / maincpu immediates
    print("\n=== ROM search for boot-sel09 FIFO immediates ===")
    img = Path("out/maincpu.bin").read_bytes()
    samples = [0x14802929, 0x1C803939, 0x03000606, 0x1A803535, 0x07800F0F, 0x09801313, 0xFFFFC000]
    for v in samples:
        pat = struct.pack("<I", v)
        hits = []
        off = 0
        while len(hits) < 6:
            i = img.find(pat, off)
            if i < 0:
                break
            hits.append(hex(i))
            off = i + 1
        print(f"  0x{v:08x} in maincpu: {hits or 'NONE'}")

    # Also search main_data bin if present
    md = Path("out/main_data.bin")
    if md.exists():
        mdb = md.read_bytes()
        print("\n=== main_data.bin search (samples) ===")
        for v in samples[:4]:
            pat = struct.pack("<I", v)
            hits = []
            off = 0
            while len(hits) < 4:
                i = mdb.find(pat, off)
                if i < 0:
                    break
                hits.append(hex(0x02000000 + i))
                off = i + 1
            print(f"  0x{v:08x} in main_data: {hits or 'NONE'}")

    # Function-port histogram if any
    print("\n=== function-port / upload absence check ===")
    for name in interesting:
        jl = OUT / f"{name}.jsonl"
        if not jl.exists():
            continue
        c = classify_jsonl(jl)
        if c["func_port"] or c["upload"] or c["fifo_n"]:
            print(f"  {name}: fifo={c['fifo_n']} func_port={c['func_port']} upload={c['upload']}")


if __name__ == "__main__":
    main()
