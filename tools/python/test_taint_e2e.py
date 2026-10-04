#!/usr/bin/env python3
"""End-to-end contract test for tools/python/taint.py on a real trace.

Validates the AGENTS.md next-work #3 contract by running the taint
pipeline on the existing ``out/trace-bit14.jsonl`` corpus artifact and
asserting that measured branch dependencies surface the expected
``fighter + 0xNNNN bit N`` form.

This test does not commit large traces; it expects an existing
trace produced by the project's reproduce script (see
``decomp/i960/tools/make_game_info_probe_scenario.py`` plus
``trace_case.py``). When the trace is missing the test skips with a
clear message rather than failing.

Run standalone (no pytest required):

    python tools/python/test_taint_e2e.py
"""

from __future__ import annotations

import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TRACE = ROOT / "out" / "trace-bit14.jsonl"
SCENARIO = ROOT / "out" / "state8-posbit6-v0727.json"
ROM_DIR = ROOT / "roms" / "vf2"
VF2I960 = ROOT / "build" / "Debug" / "vf2i960.exe"

# Acceptable fighter-window tag form (matches tools/python/taint.py output).
DEP_RE = re.compile(
    r"^fighter[0-9]+ \+ 0x[0-9a-fA-F]{4}(?: bit \d+)?$"
)
BRANCH_RE = re.compile(
    r"^branch 0x[0-9a-fA-F]{8} depends on:$"
)


def _require_artifacts() -> bool:
    missing = []
    for path in (TRACE, SCENARIO, ROM_DIR, VF2I960):
        if not path.exists():
            missing.append(str(path))
    if missing:
        print(
            "skip: taint end-to-end requires the following artifacts\n  "
            + "\n  ".join(missing)
        )
        return False
    return True


def test_taint_e2e_branch_dep_shape():
    """Run taint on a real trace and assert the contract shape holds."""
    if not _require_artifacts():
        return
    if not shutil.which("python"):
        raise SystemExit("python not on PATH")
    proc = subprocess.run(
        [
            "python",
            str(ROOT / "tools" / "python" / "taint.py"),
            "--rom-dir", str(ROM_DIR),
            "--vf2i960", str(VF2I960),
            "--scenario", str(SCENARIO),
            "--trace", str(TRACE),
        ],
        capture_output=True, text=True, timeout=180,
    )
    assert proc.returncode == 0, (
        f"taint.py failed: rc={proc.returncode}\n"
        f"stderr:\n{proc.stderr[:4000]}"
    )
    out = proc.stdout
    branch_blocks = []
    current_ip = None
    current_deps = []
    for line in out.splitlines():
        if BRANCH_RE.match(line):
            if current_ip is not None:
                branch_blocks.append((current_ip, current_deps))
            current_ip = line.split()[1]
            current_deps = []
        elif line.startswith("  "):
            tag = line.strip()
            if tag.startswith(";"):
                continue
            if tag.startswith("(no fighter"):
                continue
            current_deps.append(tag)
    if current_ip is not None:
        branch_blocks.append((current_ip, current_deps))
    assert branch_blocks, "taint produced no branch blocks at all"
    # Every dependency must match the contract form (fighter+offset[bit N]).
    for ip, deps in branch_blocks:
        for dep in deps:
            assert DEP_RE.match(dep), (
                f"branch {ip} has malformed dep: {dep!r}\n"
                f"full deps: {deps}"
            )
    # At least one branch must depend on a fighter tag in this corpus.
    fighter_dep_count = sum(
        1 for _ip, deps in branch_blocks
        for d in deps if d.startswith("fighter")
    )
    assert fighter_dep_count > 0, (
        "no branch in this trace produced a fighter dependency; "
        "either the corpus does not exercise fighter fields or the "
        "taint pipeline missed a load->compare->branch chain"
    )
    print(
        f"ok: taint end-to-end validated {len(branch_blocks)} branch blocks "
        f"with {fighter_dep_count} fighter dependencies"
    )


def main() -> int:
    test_taint_e2e_branch_dep_shape()
    print("all taint end-to-end tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())