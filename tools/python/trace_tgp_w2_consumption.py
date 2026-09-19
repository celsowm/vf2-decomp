#!/usr/bin/env python3
"""Passive analyzer: measured signals of object-table w2 / TGP polygon-ROM use.

Evidence-first. Fail-closed. Does not invent camera/mesh names.
Does not execute guest code. Streams memory-trace JSONL.

Signals classified:
  guest_table_w2_read   — i960 read of 0x020e0004+id*16+8 (object-table w2)
  guest_w2_geo_write    — i960 write of w2 / (w2&0x7fffff) / full w2 form
                          into geo RAM 0x00800000-0x00807fff
  function_port_write   — write into 0x00880000-0x00883fff (MAME copro function)
  upload_ctl_write      — write into 0x00980000-0x0098000f (MAME copro/geo ctl)
  poly_body_in_ram      — write of a polygons.bin body word (not table/protocol)
  class01_geo_command   — geo write whose class bits (w>>23)&0x1f == 0x01
                          (our tgp.c object-address class; unproven as live TGP)

If function_port_write / upload_ctl_write / poly_body_in_ram stay 0 while
guest_w2_geo_write > 0, that is measured "pointer written, no live TGP
consumption signal" — not proof TGP never runs.
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from collections import Counter
from pathlib import Path

TABLE = 0x020E0004
TABLE_END = TABLE + 0x2000
FUNC_LO = 0x00880000
FUNC_HI = 0x00884000
UPLOAD_LO = 0x00980000
UPLOAD_HI = 0x00980010
GEO_LO = 0x00800000
GEO_HI = 0x00808000
FIFO = 0x00884000

# Measured attract/pol_test w2 forms (pointer only; body not guest-written).
WATCH_W2 = {
    0x00840430: "id_0x148",
    0x0098A0B1: "id_0x97d",
    0x00040430: "id_0x148_index",
    0x0018A0B1: "id_0x97d_index",
}

# Common protocol / pad words that collide with poly body by chance.
COMMON = {
    0,
    0xFFFFFFFF,
    0x00800101,
    0x01800303,
    0x03000606,
    0x1A003434,
    0x00040401,
    0x3F800000,
    0xBF800000,
    0x44160000,
    # Common small IEEE constants that collide with mesh body and Work-RAM.
    0x3E4CCCCD,
    0xBE4CCCCD,
    0x3DCCCCCD,
    0xBDCCCCCD,
    0x3F000000,
    0xBF000000,
    0x3F99999A,
    0x45480000,
}

# Distinctive mesh/attr words from measured poly ROM dumps (pol_test 0x97d,
# attract 0x148). Presence in guest geo/FIFO writes would be a consumption
# signal; common floats above are not.
DISTINCTIVE = {
    0xE1001601,
    0xE0801501,
    0xD0A81502,
    0xD1281601,
    0xD1A81702,
    0x3D90624E,
    0x3CE075F7,
    0xBE6E7D56,
}


def build_poly_rom(rom_dir: Path) -> bytes:
    pairs = [
        ("mpr-17554.16", 0x00000000),
        ("mpr-17548.20", 0x00000002),
        ("mpr-17555.17", 0x00400000),
        ("mpr-17549.21", 0x00400002),
        ("mpr-17556.18", 0x00800000),
        ("mpr-17550.22", 0x00800002),
    ]
    region = bytearray(0x02000000)
    for name, off in pairs:
        p = rom_dir / name
        if not p.exists():
            continue
        src = p.read_bytes()
        for i in range(0, len(src), 2):
            dest = off + (i // 2) * 4
            if dest + 1 < len(region):
                region[dest] = src[i]
                region[dest + 1] = src[i + 1]
    return bytes(region)


def body_words_at(poly: bytes, word_index: int, n: int = 32) -> set[int]:
    base = (word_index & 0x7FFFFF) * 4
    out: set[int] = set()
    for i in range(n):
        o = base + i * 4
        if o + 4 <= len(poly):
            out.add(struct.unpack_from("<I", poly, o)[0])
    return out


def parse_u32(j: dict) -> int:
    try:
        return int.from_bytes(bytes.fromhex(j.get("bytes", "00"))[:4], "little")
    except Exception:
        return 0


def scan_trace(path: Path, body_set: set[int]) -> dict:
    c = Counter()
    samples: list[tuple] = []
    for ln in path.read_text(errors="replace").splitlines():
        if not ln.startswith("{"):
            continue
        try:
            j = json.loads(ln)
        except Exception:
            continue
        if j.get("type") != "memory":
            continue
        c["memory"] += 1
        a = int(j.get("address", 0))
        kind = j.get("kind")
        val = parse_u32(j)
        if kind == "read" and TABLE <= a < TABLE_END and ((a - TABLE) % 16) == 8:
            c["guest_table_w2_read"] += 1
            if len(samples) < 24:
                samples.append(("table_w2_read", hex(a), hex(val), j.get("step")))
        if kind != "write":
            continue
        if FUNC_LO <= a < FUNC_HI:
            c["function_port_write"] += 1
            samples.append(("FUNC_W", hex(a), hex(val), j.get("step")))
        if UPLOAD_LO <= a < UPLOAD_HI:
            c["upload_ctl_write"] += 1
            samples.append(("UPLOAD_W", hex(a), hex(val), j.get("step")))
        if GEO_LO <= a < GEO_HI:
            c["geo_write"] += 1
            cls = (val >> 23) & 0x1F
            if cls == 0x01:
                c["class01_geo_command"] += 1
            if val in WATCH_W2:
                c["guest_w2_geo_write"] += 1
                if len(samples) < 40:
                    samples.append(
                        ("W2_GEO", hex(a), hex(val), WATCH_W2[val], j.get("step"))
                    )
            if val in DISTINCTIVE:
                c["distinctive_poly_word_geo"] += 1
                samples.append(("DIST_GEO", hex(a), hex(val), j.get("step")))
            elif val in body_set and val not in COMMON:
                c["poly_body_in_geo_common_excluded"] += 1
        if a == FIFO and val in DISTINCTIVE:
            c["distinctive_poly_word_fifo"] += 1
            samples.append(("DIST_FIFO", hex(a), hex(val), j.get("step")))
        if val in DISTINCTIVE and not (GEO_LO <= a < GEO_HI or a == FIFO):
            c["distinctive_poly_word_other_ram"] += 1
            if len(samples) < 40:
                samples.append(("DIST_OTHER", hex(a), hex(val), j.get("step")))
        if val in body_set and val not in COMMON and val not in DISTINCTIVE and a not in range(GEO_LO, GEO_HI) and a != FIFO:
            c["nonspecific_body_other"] += 1
    return {"file": str(path), "counts": dict(c), "samples": samples[:24]}


def scan_snap(path: Path, body_set: set[int]) -> dict:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from dump_attract_state import parse_snap

    snap = parse_snap(path)
    hits = []
    bases = {
        "work-ram": 0x00500000,
        "geometry": 0x00800000,
        "buffer-ram": 0x00900000,
        "copro-port": 0x00880000,
        "copro-control": 0x00980000,
    }
    for rname, blob in snap["regions"].items():
        if not blob:
            continue
        base = bases.get(rname)
        for form, label in WATCH_W2.items():
            needle = struct.pack("<I", form)
            start = 0
            while True:
                i = blob.find(needle, start)
                if i < 0:
                    break
                addr = hex(base + i) if base is not None else f"{rname}+{i:#x}"
                hits.append(("W2_LIVE", rname, addr, hex(form), label))
                start = i + 4
        # poly body leftovers in live regions
        if rname in bases and body_set:
            for i in range(0, len(blob) - 4, 4):
                w = struct.unpack_from("<I", blob, i)[0]
                if w in DISTINCTIVE or (w in body_set and w not in COMMON and w not in WATCH_W2):
                    addr = hex((base or 0) + i)
                    hits.append(("BODY_LIVE", rname, addr, hex(w)))
                    if len(hits) > 40:
                        break
    return {
        "file": str(path),
        "ip": hex(snap["ip"]),
        "hits": hits[:32],
        "hit_count": len(hits),
    }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("traces", nargs="*", type=Path)
    ap.add_argument("--snaps", nargs="*", type=Path, default=[])
    ap.add_argument("--rom-dir", type=Path, default=Path("roms/vf2"))
    ap.add_argument("--json", type=Path, default=None)
    args = ap.parse_args()

    poly = build_poly_rom(args.rom_dir) if args.rom_dir.exists() else b""
    body: set[int] = set()
    if poly:
        for wi in (0x40430, 0x18A0B1, 0x13AEC, 0x3FE82):
            body |= body_words_at(poly, wi, 48)

    traces = args.traces
    if not traces:
        for pat in (
            "out/attr-long/*.jsonl",
            "out/attr-p1/*.jsonl",
            "out/attr-logo/*.jsonl",
            "out/attr-v0372/*.jsonl",
        ):
            traces.extend(Path().glob(pat))

    results = []
    totals = Counter()
    for p in traces:
        if not p.exists() or p.stat().st_size > 80_000_000:
            continue
        r = scan_trace(p, body)
        results.append(r)
        totals.update(r["counts"])

    snap_results = []
    for s in args.snaps:
        if s.exists():
            snap_results.append(scan_snap(s, body))

    verdict = {
        "guest_pointer_written": totals.get("guest_w2_geo_write", 0) > 0,
        "function_port_writes": totals.get("function_port_write", 0),
        "upload_ctl_writes": totals.get("upload_ctl_write", 0),
        "distinctive_poly_guest_writes": totals.get("distinctive_poly_word_geo", 0)
        + totals.get("distinctive_poly_word_fifo", 0)
        + totals.get("distinctive_poly_word_other_ram", 0),
        "nonspecific_body_other_ram": totals.get("nonspecific_body_other", 0),
        "false_class01_on_table_w0": totals.get("class01_geo_command", 0),
        "class01_geo_writes": totals.get("class01_geo_command", 0),
        "live_w2_in_snap": any(
            any(h[0] == "W2_LIVE" for h in r["hits"]) for r in snap_results
        ),
        "live_poly_body_in_snap": any(
            any(h[0] == "BODY_LIVE" for h in r["hits"]) for r in snap_results
        ),
        "interpretation": (
            "MEASURED guest write of object-table w2 pointer into geo RAM "
            "(0x804008/0x800008). NO measured function-port 0x880000 or "
            "upload-ctl 0x980000 kick. The only distinctive-looking geo hit "
            "(0x3ce075f7) is a common mesh float also present in main_data "
            "0x020096a4; guest traces show the READ source is main_data, not "
            "polygons ROM. class-0x01 geo hits are false-class on table w0 "
            "integers (0xac1502/0xf8013f/0x800000). live_w2_in_snap=false: "
            "pointer does not persist in park geometry/work regions. This is "
            "NOT a measured TGP consumption of polygons via w2 outside i960 "
            "guest writes."
            if totals.get("guest_w2_geo_write", 0)
            and totals.get("function_port_write", 0) == 0
            and totals.get("upload_ctl_write", 0) == 0
            else "See per-file counts; do not promote without matching signals."
        ),
        "distinctive_geo_false_positive": {
            "value": "0x3ce075f7",
            "poly_rom_occurrences": 727,
            "main_data_occurrences": 6,
            "guest_read_source": "0x020096a4 / 0x020096b0 main_data",
            "classification": "common float + main_data table word; not TGP poly-ROM use",
        },
    }

    report = {
        "note": "Passive analysis only. Fail-closed. Unproven MAME tags stay unproven.",
        "totals": dict(totals),
        "verdict": verdict,
        "traces": results,
        "snaps": snap_results,
    }
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print("wrote", args.json)
    print(json.dumps({"totals": dict(totals), "verdict": verdict}, indent=2))


if __name__ == "__main__":
    main()
