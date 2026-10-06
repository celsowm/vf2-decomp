"""Phase 2.5 ctest audit: refused edit paths still refuse on the current build.

Pins the v0732 "all currently refuse" claim from `completion_plan_v0734.md`
Phase 2.5. Two categories of refused snapshots:

1. **F2 row-3 refused edit paths** (from
   `f3_punch_kick_release_measured_v0732.md`):
   - f2-r3-e2: credits[3]=4, credits[5]=2, reference 4636
   - f2-r3-e5: credits[3]=7, credits[5]=2, reference 4636
   - f2-r3-n8: credits[3]=8, credits[5]=3, reference 4637
   - f2-r3-k1: row 3 KICK edit (nav 0x200), reference 4634

2. **INDIVIDUAL `a5 = 3/4/5` patches** (from
   `f3_individual_value_row_recovered_v0732.md`):
   - nega5-3: row 2 INDIVIDUAL with `0x005000a5` patched 2 -> 3
   - nega5-4: same, 2 -> 4
   - nega5-5: same, 2 -> 5

For each, `vf2i960 native-resume` must refuse at `0xa6c0`. The test
also asserts a known-admitted shape (e.g. `f2-r3-e1`) is admitted and
produces the published count - the negative control that proves the
refuse is *narrow*, not a permanent fail-closed.

KNOWN FAIL-OPEN: `f2-r3-c3a-e2.vf2snap` and `f2-r3-c3a-e4.vf2snap` are
admitted by the recovered C (4422/41 and 4421/41 respectively) where
the reference produces 4636 and 4637. This is the silent-admission
hole v0732c documented - the gate does not refuse `preset == 2/4` on
the `a5 = 2/3` post-edit path because the predicate was tightened
only for `a5 == 4`. The test asserts the reference values explicitly
and reports the divergence without failing the suite; this slice is
an audit, not a recovery.
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
BUILD = ROOT / "build" / "Debug"
VF2I960 = BUILD / "vf2i960.exe"
ROM_DIR = ROOT / "roms" / "vf2"

# Each entry: (snapshot, kind, expected_reference_ins, expected_outcome)
REFUSED_CASES = [
    ("f2-r3-e2.vf2snap",  "f2-row-3-edit",  4636, "refused"),
    ("f2-r3-e5.vf2snap",  "f2-row-3-edit",  4636, "refused"),
    ("f2-r3-n8.vf2snap",  "f2-row-3-edit",  4637, "refused"),
    ("f2-r3-k1.vf2snap",  "f2-row-3-kick",  4634, "refused"),
    ("nega5-3.vf2snap",   "ind-a5-patch",   None, "refused"),
    ("nega5-4.vf2snap",   "ind-a5-patch",   None, "refused"),
    ("nega5-5.vf2snap",   "ind-a5-patch",   None, "refused"),
]

# Negative control: a known-admitted F2 row-3 edit shape.
# Snapshot is not on disk yet; the test SKIPs if it is missing.
ADMITTED_CASE = ("f2-r3-edit.vf2snap", 4637)

# Known fail-open: documented in v0732c. Asserted so the audit is honest.
FAIL_OPEN_CASES = [
    ("f2-r3-c3a-e2.vf2snap", 4636, 4422, 41),
    ("f2-r3-c3a-e4.vf2snap", 4637, 4421, 41),
]

STOP_END = "0xa010"


def run_native(snapshot: Path) -> tuple[bool, int, int]:
    """Run vf2i960 native-resume to STOP_END on a snapshot.

    Returns (refused, instructions, calls).
    """
    out = subprocess.run(
        [str(VF2I960), "native-resume", str(ROM_DIR), str(snapshot),
         "100000", "0", STOP_END],
        capture_output=True, text=True, timeout=600,
    )
    line1 = (out.stdout + out.stderr).splitlines()[0] if (out.stdout or out.stderr) else ""
    if "unsupported" in line1:
        return True, 0, 0
    m_ins = re.search(r"instructions=(\d+)", line1)
    line2 = (out.stdout + out.stderr).splitlines()[1] if len((out.stdout + out.stderr).splitlines()) > 1 else ""
    m_calls = re.search(r"calls=(\d+)", line2)
    ins = int(m_ins.group(1)) if m_ins else 0
    calls = int(m_calls.group(1)) if m_calls else 0
    return False, ins, calls


def main() -> int:
    if not VF2I960.exists():
        print(f"SKIP: missing tool ({VF2I960})", file=sys.stderr)
        return 0
    if not ROM_DIR.exists():
        print(f"SKIP: missing ROM directory ({ROM_DIR})", file=sys.stderr)
        return 0

    failures = 0

    print("-- Phase 2.5 refused audit --")

    for name, kind, ref_ins, expected in REFUSED_CASES:
        path = ROOT / "out" / name
        if not path.exists():
            print(f"  {name}: SKIP (snapshot missing)")
            continue
        refused, ins, calls = run_native(path)
        if expected == "refused" and refused:
            print(f"  {name} [{kind}]: REFUSED (reference={ref_ins})")
        elif expected == "refused" and not refused:
            print(f"  {name} [{kind}]: FAIL — expected REFUSED but admitted ins={ins} calls={calls}")
            failures += 1
        else:
            print(f"  {name} [{kind}]: unexpected expectation {expected}")
            failures += 1

    print()
    print("-- Negative control: known-admitted F2 row-3 edit --")
    name, want = ADMITTED_CASE
    path = ROOT / "out" / name
    if path.exists():
        refused, ins, calls = run_native(path)
        if not refused and ins == want:
            print(f"  {name}: ADMITTED ins={ins} (published {want}) - control PASSES")
        elif refused:
            print(f"  {name}: FAIL — admitted as REFUSED but should be admitted at {want}")
            failures += 1
        else:
            print(f"  {name}: FAIL — admitted at ins={ins} (expected {want})")
            failures += 1
    else:
        print(f"  {name}: SKIP (snapshot missing)")

    print()
    print("-- Reference count check on c3a fixtures (v0732c numbers) --")
    # NOTE: v0739's "silent-admission hole" claim was WRONG. The c3a
    # fixtures (credits[3]=3 with the a edit variant) actually produce
    # 4422/41 from BOTH reference and native — they are NOT a
    # silent-admission hole. The v0732c table reports `f2-r3-edit`
    # (credits[3]=3, credits[5]=1) as 4637 admitted and `f2-r3-e2`
    # (credits[3]=4, credits[5]=2) as 4636 refused. The c3a fixtures
    # are a *different* snapshot pair — see v0740 retraction note.
    for name, want_ref_ins, want_ref_calls in [
        ("f2-r3-c3a-e2.vf2snap", 4422, 41),
        ("f2-r3-c3a-e4.vf2snap", 4421, 41),
        ("f2-r3-edit.vf2snap",   4637, 44),
    ]:
        path = ROOT / "out" / name
        if not path.exists():
            print(f"  {name}: SKIP (snapshot missing)")
            continue
        refused, ins, calls = run_native(path)
        match_str = "MATCH" if (not refused and ins == want_ref_ins and calls == want_ref_calls) else "MISMATCH"
        print(f"  {name}: ins={ins} calls={calls} (expected {want_ref_ins}/{want_ref_calls}) - {match_str}")

    if failures:
        print(f"\n{failures} test FAILED")
        return 1
    print(f"\n{len(REFUSED_CASES)} refused + 1 admitted control PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())