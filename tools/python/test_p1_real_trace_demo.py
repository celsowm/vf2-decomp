#!/usr/bin/env python3
"""Regression test for the P1 real-trace factory chain demo.

The demo runs the public ``Frontier`` API against the actual
``out/trace-both.jsonl`` + ``out/trace-f0.jsonl`` corpus and locks in
the invariants documented in
``decomp/i960/notes/p1_fighter_bases_retraction_v0732k.md``:

- contiguous 4B block at ``0x0d00..0x0ee0``
- length 120, byte_size 480
- ip_overlap 1.0
- top_ips ``[0x2399c, 0x23a38]``
- base_count **2** - the block IS dual-base
- reads 480 / writes 480 (240 each per fighter base)

The block offset is expressed from the MEASURED fighter0 base ``0x510980``.
An earlier revision of this test used ``0x510000`` and read the block at
``0x1680`` with ``base_count == 1``. That was the same 120 offsets
displaced by 0x980, and the wrong fighter1 window (0x520000) missed the
real fighter1 struct at 0x512980 entirely - so the "pending" claim was an
artefact of the base, not a property of the corpus.

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
    assert "0x00000d00..0x00000ee0" in out, (
        f"demo did not surface the expected 0xd00..0xee0 block:\n"
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
    # The promotion this whole slice existed to unblock. Asserting base_count
    # == 1 here is what kept the wrong fighter bases alive from v0729 to
    # v0732k, so the assertion is deliberately the strong one.
    assert "base_count=2" in out, (
        f"demo did not surface base_count=2 (dual-base promotion):\n"
        f"{out[:4000]}"
    )
    assert "DUAL-BASE" in out, (
        f"demo did not report the block as promoted:\n{out[:4000]}"
    )
    assert "144 of them dual-base" in out, (
        f"demo did not report the per-offset dual-base roll-up:\n"
        f"{out[:4000]}"
    )
    print(
        "ok: p1_real_trace_demo reproduced the 0xd00..0xee0 block "
        "with all documented invariants on the real corpus, and the "
        "block is dual-base (base_count=2)"
    )


def main() -> int:
    test_p1_real_trace_demo_reproduces_block()
    print("all p1_real_trace_demo tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())