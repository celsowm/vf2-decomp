#!/usr/bin/env python3
"""Step attract sel3 phases by zeroing worker counters + frame-dispatch visit."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_phase8_state import main as dump_main  # noqa
from dump_attract_state import parse_snap, wu8, wu32

I960 = "build/Debug/vf2i960.exe"
PROBE = "build/Debug/vf2probe.exe"
ROM = "roms/vf2"
PTR50 = 0x00515B50  # [0x500834]+0x50
MASK28 = 0x00500028  # write 0x00030001 to keep sel=3 + mask=1
NAV = 0x00500704


def resume(cur: Path, out: Path, waddr: int, wval: int, steps: int = 0) -> None:
    cmd = [
        I960, "resume-trace", ROM, str(cur), str(steps),
        "0xffffffff", "0xffffffff",
        hex(waddr), str(wval), str(out),
    ]
    subprocess.run(cmd, capture_output=True, text=True)


def probe_ip(cur: Path, out: Path) -> None:
    cmd = [
        PROBE, "--rom-dir", ROM, "--snapshot", str(cur),
        "--set-ip", "0x0000a6c0", "--max-steps", "1",
        "--until", "0x0000a6c0", "--set-u8", "0x00500704=0",
        "--output-snapshot", str(out),
    ]
    subprocess.run(cmd, capture_output=True, text=True)


def resume_frame(cur: Path, out: Path, steps: int = 200000) -> dict:
    cmd = [
        I960, "resume-trace", ROM, str(cur), str(steps),
        "0xffffffff", "0xffffffff",
        hex(NAV), "0", str(out), "0x0000a6c0",
    ]
    p = subprocess.run(cmd, capture_output=True, text=True)
    text = p.stdout + p.stderr
    rec = {"stop": "stop address reached" in text, "unsup": "unsupported operation" in text}
    if out.exists():
        s = parse_snap(out)
        w = s["regions"]["work-ram"]
        rec.update({
            "ip": s["ip"],
            "sel": wu8(w, 0x50002A),
            "ph": wu8(w, 0x500030),
            "ready": wu32(w, 0x550000),
            "mask28": wu32(w, 0x500028),
            "ctr50": wu32(w, PTR50) if 0x500000 <= PTR50 < 0x600000 else None,
        })
        # tile strings quick
        from dump_attract_state import tile_strings
        rec["tiles"] = tile_strings(s["regions"]["tile-ram"], 60)
        rec["nz_tex"] = sum(
            1 for c in s["regions"]["texture-ram0"][:0x10000] if c not in (0, 0xFF)
        )
    return rec


def main() -> None:
    cur = Path(sys.argv[1] if len(sys.argv) > 1 else "out/attr-long/p10-ip.vf2snap")
    outdir = Path("out/attr-phases")
    outdir.mkdir(parents=True, exist_ok=True)
    for i in range(12):
        # ensure sel=3 and counters ready for next worker
        w_ctr = outdir / f"s{i:02d}-ctr.vf2snap"
        w_ip = outdir / f"s{i:02d}-ip.vf2snap"
        w_out = outdir / f"s{i:02d}-frame.vf2snap"
        resume(cur, w_ctr, PTR50, 1)
        resume(w_ctr, w_out, MASK28, 0x00030001)  # temp; probe next
        # actually chain: ctr write -> mask write -> probe ip -> frame
        w_mask = outdir / f"s{i:02d}-mask.vf2snap"
        resume(w_ctr, w_mask, MASK28, 0x00030001)
        probe_ip(w_mask, w_ip)
        rec = resume_frame(w_ip, w_out)
        print(
            f"step{i:02d} from={cur.name} stop={rec.get('stop')} unsup={rec.get('unsup')} "
            f"ip=0x{rec.get('ip',0):08x} sel=0x{rec.get('sel',0):02x} "
            f"ph=0x{rec.get('ph',0):02x} ready={rec.get('ready')} "
            f"mask28=0x{rec.get('mask28',0):08x} ctr50={rec.get('ctr50')} "
            f"nz_tex={rec.get('nz_tex')} tiles={rec.get('tiles')!r}",
            flush=True,
        )
        if not w_out.exists():
            break
        cur = w_out
        ph = rec.get("ph", 0)
        if rec.get("sel") == 0x11 and rec.get("tiles", "").startswith("TEST"):
            print("TEST MENU reached", flush=True)
            break
        if ph and ph >= 0x12:
            print("phase>=0x12", flush=True)


if __name__ == "__main__":
    main()
