#!/usr/bin/env python3
"""v0372 campaign: alternative selectors / ready / board / mode on attract parks.

Measure-only. Fail-closed: never invent logo/mesh semantics.
Writes compact JSONL + stdout summary under out/attr-v0372/.
"""
from __future__ import annotations

import json
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_attract_state import parse_snap, wu8, wu32, tile_strings, sha16

PROBE = "build/Debug/vf2probe.exe"
ROM = "roms/vf2"
OUT = Path("out/attr-v0372")

# Model 2A observed windows
FIFO = 0x00884000
FUNC = 0x00880000
GEO_LO = 0x00800000
GEO_HI = 0x00808000
UPLOAD = 0x00980000
TILE_LO = 0x01000000
TILE_HI = 0x02000000
ROM_LO = 0x00000000
ROM_HI = 0x00200000
MAIN_DATA_LO = 0x02000000
MAIN_DATA_HI = 0x04000000

# Coherent sel/mask/mode layout (LE):
# 0x500028 u32 = mask_byte | 0 | sel | mode   e.g. 0x03030001 => mask=1 sel=3 mode=3
# Prefer u8 writes so adjacent fields stay coherent.
SEL = 0x0050002A
MODE = 0x0050002B
MASK28 = 0x00500028
MASK2C = 0x0050002C
PHASE = 0x00500030
READY = 0x00550000
BOARD = 0x00508000
NAV = 0x00500704
CD = 0x00500024
FLAGS68 = 0x00500068


def run_probe(src, dst, sets, ip, until, steps, mem=False) -> str:
    cmd = [
        PROBE, "--rom-dir", ROM, "--snapshot", str(src),
        "--set-ip", ip, "--until", until, "--max-steps", steps,
        "--output-snapshot", str(dst),
    ]
    for s in sets:
        if s.startswith("u8:"):
            cmd += ["--set-u8", s[3:]]
        elif s.startswith("u16:"):
            cmd += ["--set-u16", s[4:]]
        else:
            cmd += ["--set-u32", s]
    if mem:
        cmd.append("--memory-trace")
    p = subprocess.run(cmd, capture_output=True, text=True)
    return p.stdout + p.stderr


def state_line(path: Path) -> dict:
    if not path.exists():
        return {"missing": str(path)}
    s = parse_snap(path)
    w = s["regions"]["work-ram"]
    t = s["regions"]["tile-ram"]
    x = s["regions"]["texture-ram0"]
    g = s["regions"]["geometry"]
    b = s["regions"]["buffer-ram"]
    return {
        "file": path.name,
        "ip": f"0x{s['ip']:08x}",
        "executed": s["executed"],
        "sel": wu8(w, SEL),
        "mode2b": wu8(w, MODE),
        "mask28": f"0x{wu32(w, MASK28):08x}",
        "mask2c": f"0x{wu32(w, MASK2C):08x}",
        "ph": wu8(w, PHASE),
        "ready": wu32(w, READY),
        "board": f"0x{wu32(w, BOARD):08x}",
        "nav": f"0x{wu32(w, NAV):08x}",
        "cd": wu32(w, CD),
        "flags68": f"0x{wu32(w, FLAGS68):08x}",
        "tiles": tile_strings(t, 80),
        "tex_sha": sha16(x),
        "nz_tex64k": sum(1 for c in x[:0x10000] if c not in (0, 0xFF)),
        "geom_sha": sha16(g),
        "geom_nz": sum(
            1
            for i in range(0, min(len(g), 0x8000), 4)
            if int.from_bytes(g[i : i + 4], "little") != 0
        ),
        "buf_sha": sha16(b),
    }


def classify_trace(text: str) -> dict:
    writes = Counter()
    reads = Counter()
    fifo_vals = Counter()
    fifo_funcs = Counter()
    func_vals = Counter()
    upload_vals = Counter()
    geo_reads = Counter()
    geo_writes = Counter()
    tile_writes = 0
    poly_like_reads = Counter()  # ROM/main_data reads that look like tables
    model_like = Counter()
    steps = 0
    halt = None
    final_ip = None
    for ln in text.splitlines():
        if not ln.startswith("{"):
            continue
        try:
            j = json.loads(ln)
        except Exception:
            continue
        if j.get("type") == "step":
            steps += 1
        if j.get("type") == "final":
            halt = j.get("halt_reason")
            final_ip = j.get("ip")
        if j.get("type") != "memory":
            continue
        a = j.get("address", 0)
        bs = bytes.fromhex(j.get("bytes") or "00")
        val = int.from_bytes(bs, "little") if bs else 0
        if j.get("kind") == "write":
            writes[a] += 1
            if a == FIFO:
                fifo_vals[val] += 1
                fifo_funcs[(val >> 23) & 0x3F] += 1
            elif a == FUNC:
                func_vals[val] += 1
            elif UPLOAD <= a < UPLOAD + 0x20000:
                upload_vals[val] += 1
            elif GEO_LO <= a < GEO_HI:
                geo_writes[a] += 1
            elif TILE_LO <= a < TILE_HI:
                tile_writes += 1
        elif j.get("kind") == "read":
            reads[a] += 1
            if GEO_LO <= a < GEO_HI:
                geo_reads[a] += 1
            if MAIN_DATA_LO <= a < MAIN_DATA_HI:
                poly_like_reads[val] += 1
            if ROM_LO <= a < ROM_HI:
                # stable absolute addresses that could be tables
                if a in (0x02A69CD2, 0x02A69E4A, 0x02A69EE6, 0x02A69F4C, 0x02A69F02, 0x02A6C15E):
                    model_like[a] += 1
                if 0x02A60000 <= a <= 0x02A80000 or 0x02A00000 <= val <= 0x02B00000:
                    model_like[f"rom@{a:#x}"] += 1
    return {
        "steps": steps,
        "halt": halt,
        "final_ip": final_ip,
        "fifo_writes": sum(fifo_vals.values()),
        "fifo_funcs": sorted(fifo_funcs.items(), key=lambda x: -x[1])[:16],
        "fifo_top": [(f"0x{v:08x}", n) for v, n in fifo_vals.most_common(8)],
        "func_writes": sum(func_vals.values()),
        "func_top": [(f"0x{v:08x}", n) for v, n in func_vals.most_common(8)],
        "upload_writes": sum(upload_vals.values()),
        "upload_top": [(f"0x{v:08x}", n) for v, n in upload_vals.most_common(8)],
        "geo_writes": sum(geo_writes.values()),
        "geo_reads": sum(geo_reads.values()),
        "geo_top_addrs": [(f"0x{a:08x}", n) for a, n in geo_writes.most_common(6)]
        + [("R", 0)]
        + [(f"0x{a:08x}", n) for a, n in geo_reads.most_common(6)],
        "tile_writes": tile_writes,
        "top_writes": [(f"0x{a:08x}", n) for a, n in writes.most_common(10)],
        "top_reads": [(f"0x{a:08x}", n) for a, n in reads.most_common(10)],
        "maindata_vals": [(f"0x{v:08x}", n) for v, n in poly_like_reads.most_common(8)],
        "model_like": [(k, n) for k, n in model_like.most_common(8)],
    }


def coherent_sel_sets(sel: int, mask: int = 1, mode: int | None = None, extra=None) -> list[str]:
    """Write sel carefully via u8; keep mask28 u32 coherent (mask | sel<<16 | mode<<24)."""
    m = sel if mode is None else mode
    u32v = (m << 24) | (sel << 16) | (mask & 0xFF)
    sets = [
        f"u8:0x{SEL:08x}={sel}",
        f"u8:0x{MODE:08x}={m}",
        f"0x{MASK28:08x}=0x{u32v:08x}",
    ]
    if extra:
        sets.extend(extra)
    return sets


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    src = Path("out/attr-fs/all0-ready1.vf2snap")
    boot = Path("out/park-after-irq.vf2snap")
    print("SRC baselines")
    recs = []
    for p in (src, boot, Path("out/sixth-fresh.vf2snap")):
        if p.exists():
            rec = state_line(p)
            print(json.dumps(rec, ensure_ascii=False))
            recs.append({"kind": "baseline", **rec})

    # --- Case matrix ---
    # A) Alternative selectors from phase14 park, ready=0, nav=0, board bit9 clear
    #    Force worker entry at frame dispatch 0xa6c0.
    cases = []

    for sel in (0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 19):
        cases.append(
            (
                f"sel{sel:02d}-r0-a6c0",
                src,
                coherent_sel_sets(
                    sel,
                    mask=1,
                    mode=sel,
                    extra=[
                        f"u8:0x{READY:08x}=0",
                        f"u8:0x{NAV:08x}=0",
                        f"0x{BOARD:08x}=0",
                        f"0x{0x005502C0:08x}=0",
                        f"0x{0x005502D0:08x}=0",
                        f"0x{0x005502E0:08x}=0",
                    ],
                ),
                "0x0000a6c0",
                "0x0004d000",
                "20000",
            )
        )

    # B) ready=1 vs 0 on sel3 phase14 + mode overrides
    for ready, mode, name in (
        (0, 0x03, "sel03-r0-mode03"),
        (1, 0x03, "sel03-r1-mode03"),
        (0, 0x0C, "sel03-r0-mode0c"),
        (0, 0x0D, "sel03-r0-mode0d"),
        (1, 0x0C, "sel03-r1-mode0c"),
        (0, 0x00, "sel03-r0-mode00"),
        (0, 0x10, "sel03-r0-mode10"),
        (0, 0x11, "sel03-r0-mode11"),
    ):
        cases.append(
            (
                name,
                src,
                [
                    f"u8:0x{SEL:08x}=3",
                    f"u8:0x{MODE:08x}={mode}",
                    f"0x{MASK28:08x}=0x{(mode << 24) | 0x00030000 | 1:08x}",
                    f"u8:0x{READY:08x}={ready}",
                    f"u8:0x{NAV:08x}=0",
                    f"0x{BOARD:08x}=0",
                ],
                "0x0000a6c0",
                "0x0004d000",
                "40000",
            )
        )

    # C) board bit9 set vs clear + forced phases on sel3
    cases += [
        (
            "sel03-ph0e-bit9set",
            src,
            [
                f"u8:0x{SEL:08x}=3",
                f"u8:0x{MODE:08x}=3",
                f"0x{MASK28:08x}=0x03030001",
                f"u8:0x{PHASE:08x}=0x0e",
                f"u8:0x{READY:08x}=0",
                f"u8:0x{NAV:08x}=0",
                f"0x{BOARD:08x}=0x00008a00",
            ],
            "0x0000a6c0",
            "0x0004d000",
            "40000",
        ),
        (
            "sel09-ph00-r0",
            src,
            coherent_sel_sets(
                9,
                mask=1,
                mode=9,
                extra=[
                    f"u8:0x{PHASE:08x}=0",
                    f"u8:0x{READY:08x}=0",
                    f"u8:0x{NAV:08x}=0",
                    f"0x{BOARD:08x}=0",
                ],
            ),
            "0x0000a6c0",
            "0x0004d000",
            "40000",
        ),
        (
            "sel09-worker-ph00",
            src,
            [
                f"u8:0x{SEL:08x}=9",
                f"u8:0x{MODE:08x}=9",
                f"0x{MASK28:08x}=0x09090001",
                f"u8:0x{PHASE:08x}=0",
                f"u8:0x{READY:08x}=0",
            ],
            "0x0000d380",
            "0x0000d4ac",
            "2000",
        ),
        (
            "sel09-ph00-cfbc",
            src,
            [
                f"u8:0x{SEL:08x}=9",
                f"u8:0x{MODE:08x}=9",
                f"u8:0x{PHASE:08x}=0",
                f"u8:0x{READY:08x}=0",
            ],
            "0x0000d4ac",
            "0x0000d844",
            "8000",
        ),
        (
            "sel06-worker",
            src,
            [
                f"u8:0x{SEL:08x}=6",
                f"u8:0x{MODE:08x}=6",
                f"0x{MASK28:08x}=0x06060001",
                f"u8:0x{READY:08x}=0",
            ],
            "0x0000c474",
            "0x0000c7ec",
            "20000",
        ),
        (
            "sel10-worker",
            src,
            [
                f"u8:0x{SEL:08x}=10",
                f"u8:0x{MODE:08x}=10",
                f"0x{MASK28:08x}=0x0a0a0001",
                f"u8:0x{READY:08x}=0",
            ],
            "0x00010088",
            "0x0001018c",
            "4000",
        ),
        (
            "sel16-worker",
            src,
            [
                f"u8:0x{SEL:08x}=16",
                f"u8:0x{MODE:08x}=16",
                f"0x{MASK28:08x}=0x10100001",
                f"u8:0x{READY:08x}=0",
            ],
            "0x00010a0c",
            "0x00010b5c",
            "8000",
        ),
        # Boot path: park-after-irq already sel=0, signature a5a5... (draw path)
        (
            "boot-sel00-natural",
            boot,
            [],
            "0x0000a6c0",
            "0x0004d000",
            "20000",
        ),
        (
            "boot-sel03-force",
            boot,
            coherent_sel_sets(
                3,
                mask=0,
                mode=3,
                extra=[f"u8:0x{READY:08x}=0", f"u8:0x{NAV:08x}=0"],
            ),
            "0x0000a6c0",
            "0x0004d000",
            "20000",
        ),
        (
            "boot-sel09-force",
            boot,
            coherent_sel_sets(
                9,
                mask=0,
                mode=9,
                extra=[f"u8:0x{READY:08x}=0", f"u8:0x{NAV:08x}=0"],
            ),
            "0x0000a6c0",
            "0x0004d000",
            "20000",
        ),
        (
            "boot-sel00-sigclear",
            boot,
            [
                "0x0059cfe0=0",
                "0x0059cfe4=0",
                "0x0059cfe8=0",
                "0x0059cfec=0",
                f"u8:0x{SEL:08x}=0",
                f"u8:0x{MODE:08x}=0",
                f"0x{MASK28:08x}=0",
            ],
            "0x0000a6c0",
            "0x0004d000",
            "20000",
        ),
    ]

    summary = []
    for name, srcp, sets, ip, until, steps in cases:
        if not srcp.exists():
            print("missing src", srcp)
            continue
        dst = OUT / f"{name}.vf2snap"
        print(f"\n=== {name} src={srcp.name} ip={ip} until={until} steps={steps}")
        print("  sets:", sets)
        text = run_probe(srcp, dst, sets, ip, until, steps, mem=True)
        (OUT / f"{name}.jsonl").write_text(
            "\n".join(ln for ln in text.splitlines() if ln.startswith("{")) + "\n"
        )
        cls = classify_trace(text)
        st = state_line(dst)
        print("  state:", json.dumps(st, ensure_ascii=False))
        print("  class:", json.dumps(cls, ensure_ascii=False))
        summary.append({"name": name, "src": srcp.name, "sets": sets, "state": st, "class": cls})

    (OUT / "summary.json").write_text(json.dumps(summary, indent=2, ensure_ascii=False))
    (OUT / "baselines.json").write_text(json.dumps(recs, indent=2, ensure_ascii=False))
    print(f"\nWrote {OUT}/summary.json ({len(summary)} cases)")

    # Quick tile unique scan
    print("\n=== tile uniqueness vs baselines ===")
    base_tiles = {r["tiles"] for r in recs}
    for s in summary:
        tl = s["state"].get("tiles") or ""
        if tl and tl not in base_tiles:
            print(f"  UNIQUE tiles {s['name']}: {tl!r}")
        sel = s["state"].get("sel")
        ph = s["state"].get("ph")
        fifo = s["class"].get("fifo_writes")
        if fifo and fifo > 0:
            print(
                f"  FIFO {s['name']}: sel={sel} ph={ph} fifo={fifo} "
                f"funcs={s['class'].get('fifo_funcs')} "
                f"funcw={s['class'].get('func_writes')} "
                f"up={s['class'].get('upload_writes')} "
                f"geoW={s['class'].get('geo_writes')} geoR={s['class'].get('geo_reads')} "
                f"tileW={s['class'].get('tile_writes')}"
            )


if __name__ == "__main__":
    main()
