#!/usr/bin/env python3
"""P1 calibration renderer — out/attr-render/v0377/p1/

Host hypothesis only (skip3_float_link hex-calibrated). Not recovered C.
Filenames include decode mode. No invented mesh names.
"""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

OUT = Path("out/attr-render/v0377/p1")
IDS = "0x97d,0x985,0x148,0x14f,0x08f"
SIZE = 512
RANK = Path("out/attr-p1/oracle_decode_score.json")
BEST = "skip3_float_link"


def main() -> int:
    best = BEST
    if RANK.exists():
        rep = json.loads(RANK.read_text())
        # Prefer explicit hex-calibrated choice; do not follow auto span-winners
        best = rep.get("chosen_mode") or BEST
        if rep.get("vertex_stream_corroboration", "").startswith("ABSENT"):
            # fail-closed: keep hex hypothesis even if auto rank differs
            if best not in ("skip3_float_link", "skip3"):
                best = BEST
    OUT.mkdir(parents=True, exist_ok=True)
    cmd = [
        sys.executable,
        "tools/python/render_mesh_host.py",
        "--id", IDS,
        "--out", str(OUT),
        "--size", str(SIZE),
        "--decode", best,
        "--no-contact",
    ]
    print("running:", " ".join(cmd))
    p = subprocess.run(cmd)
    files = sorted(OUT.glob("id_*.png"))
    print(f"png_count={len(files)} out={OUT}")
    for f in files[:60]:
        print(" ", f.name)
    (OUT / "render_p1_manifest.json").write_text(
        json.dumps(
            {
                "ids": IDS,
                "size": SIZE,
                "decode": best,
                "pngs": [f.name for f in files],
                "note": (
                    "Host hypothesis renders (skip3_float_link hex-calibrated). "
                    "Oracle vertex-stream corroboration ABSENT/UNPROVEN. "
                    "Not recovered C, not a named logo."
                ),
            },
            indent=2,
        )
    )
    return p.returncode


if __name__ == "__main__":
    raise SystemExit(main())
