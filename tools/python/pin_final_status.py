#!/usr/bin/env python3
"""Oracle pin: final-status 0x4bf90 clear of 0x550000."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dump_video_ready import main as dump_ready  # noqa
from dump_attract_state import parse_snap, wu8, wu32

PROBE = "build/Debug/vf2probe.exe"
ROM = "roms/vf2"


def run(src: Path, dst: Path, ip: str, sets: list[str], until: str, steps: str) -> None:
    cmd = [
        PROBE, "--rom-dir", ROM, "--snapshot", str(src),
        "--set-ip", ip, "--until", until, "--max-steps", steps,
        "--output-snapshot", str(dst),
    ]
    for s in sets:
        cmd += ["--set-u32", s]
    p = subprocess.run(cmd, capture_output=True, text=True)
    print(f"--- {src.name} -> {dst.name} ip={ip} until={until}")
    print((p.stdout + p.stderr)[-500:])


def state(path: Path) -> None:
    if not path.exists():
        print(" missing", path)
        return
    s = parse_snap(path)
    w = s["regions"]["work-ram"]
    print(
        f"  {path.name} ip=0x{s['ip']:08x} sel=0x{wu8(w,0x50002A):02x} "
        f"ph=0x{wu8(w,0x500030):02x} ready=0x{wu32(w,0x550000):08x} "
        f"ctr0=0x{wu32(w,0x5502C0):08x} ctr1=0x{wu32(w,0x5502D0):08x} "
        f"ctr2=0x{wu32(w,0x5502E0):08x} board=0x{wu32(w,0x508000):08x} "
        f"status55c2f0=0x{wu32(w,0x55C2F0):08x}"
    )


def main() -> None:
    src = Path(sys.argv[1] if len(sys.argv) > 1 else "out/attr-phases/s04-frame.vf2snap")
    outdir = Path("out/attr-fs")
    outdir.mkdir(parents=True, exist_ok=True)

    cases = [
        (
            "all0-ready1",
            ["0x005502c0=0", "0x005502d0=0", "0x005502e0=0", "0x00550000=1"],
        ),
        (
            "ctr2=1-ready1",
            ["0x005502c0=0", "0x005502d0=0", "0x005502e0=1", "0x00550000=1"],
        ),
        (
            "ctr0=1-ready1",
            ["0x005502c0=1", "0x005502d0=0", "0x005502e0=0", "0x00550000=1"],
        ),
    ]
    for name, sets in cases:
        dst = outdir / f"{name}.vf2snap"
        run(src, dst, "0x0004bf90", sets, "0x0004bfdc", "200")
        state(dst)

    # After clear pin, try phase14 worker with ready forced 0
    cleared = outdir / "all0-ready1.vf2snap"
    if cleared.exists():
        dst = outdir / "p14-after-clear.vf2snap"
        run(
            cleared,
            dst,
            "0x0000c0a4",
            ["0x00550000=0", "0x00515b50=0"],
            "0x0004bfdc",
            "500",
        )
        # also try until a6c0
        dst2 = outdir / "p14-a6c0.vf2snap"
        run(
            cleared,
            dst2,
            "0x0000a6c0",
            ["0x00500704=0", "0x00550000=0"],
            "0x0000a6c0",
            "100000",
        )
        state(dst)
        state(dst2)


if __name__ == "__main__":
    main()
