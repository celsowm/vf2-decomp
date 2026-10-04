#!/usr/bin/env python3
"""Regression test for the P1 real-trace factory chain demo.

The demo runs the public ``Frontier`` API against the actual
``out/trace-both.jsonl`` + ``out/trace-f0.jsonl`` corpus and locks in
the same invariants documented in
``decomp/i960/notes/p1_player_0x1680_block_stability_v0730.md``:

- contiguous 4B block at ``0x1680..0x1860``
- length 120, byte_size 480
- ip_overlap 1.0
- top_ips ``[0x2399c, 0x23a38]``
- base_count 1 (fighter1-inclusive trace not on master)

The test skips cleanly when the corpus traces are missing (the
traces are gitignored ROM-backed artefacts). It does NOT regenerate
them.

Run standalone:

    python tools/python/test_p1_real_trace_demo.py
"""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEMO = ROOT / "tools" / "python" / "p1_real_trace_demo.py"


def test_p1_real_trace_demo_reproduces_block():
    """Run the real-trace demo and assert it exits 0 with PASS."""
    if not shutil.which("python"):
        raise SystemExit("python not on PATH")
    proc = subprocess.run(
        ["python", str(DEMO)],
        capture_output=True, text=True, timeout=60,
    )
    assert proc.returncode == 0, (
        f"p1_real_trace_demo failed: rc={proc.returncode}\n"
        f"stdout:\n{proc.stdout[:4000]}\n"
        f"stderr:\n{proc.stderr[:4000]}"
    )
    out = proc.stdout
    # The expected invariants MUST appear in the output.
    assert "PASS:" in out, (
        f"demo did not print PASS line:\n{out[:4000]}"
    )
    assert "0x00001680..0x00001860" in out, (
        f"demo did not surface the expected 0x1680..0x1860 block:\n"
        f"{out[:4000]}"
    )
    assert "length=120" in out, (
        f"demo did not surface the expected length=120:\n{out[:4000]}"
    )
    assert "ip_overlap=1.0" in out, (
        f"demo did not surface ip_overlap=1.0:\n{out[:4000]}"
    )
    assert "top_ips=['0x0002399c', '0x00023a38']" in out, (
        f"demo did not surface the expected top_ips pair:\n"
        f"{out[:4000]}"
    )
    assert "base_count=1" in out, (
        f"demo did not surface base_count=1:\n{out[:4000]}"
    )
    print(
        "ok: p1_real_trace_demo reproduced the 0x1680..0x1860 block "
        "with all documented invariants on the real corpus"
    )


def main() -> int:
    test_p1_real_trace_demo_reproduces_block()
    print("all p1_real_trace_demo tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())