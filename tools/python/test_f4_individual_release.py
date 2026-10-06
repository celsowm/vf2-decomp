"""F4 INDIVIDUAL post-edit release: native vs reference differential.

Closes Phase 2.6 / F4 of `completion_plan_v0734.md`. The frontier is
INDIVIDUAL walks at rows 1-2 only (rows 3-5 are unreachable by walking
in INDIVIDUAL mode). This script pins the three proven releases:

| snapshot    | description                  | reference | native   |
|-------------|------------------------------|-----------|--------------|
| indk-c.vf2snap  | INDIVIDUAL row-1 KICK release  | 4293/38 | 4293/38 |
| indp-c.vf2snap  | INDIVIDUAL row-1 PUNCH release | 4293/38 | 4293/38 |
| indk2-c.vf2snap | INDIVIDUAL row-2 KICK release  | 4294/38 | 4294/38 |

The `vf2probe --until 0xa010` leg is the i960 reference executor;
the `vf2i960 native-resume` leg is the recovered native. Both legs
start from the same .vf2snap and exit at 0xa010; the differential
contract is instruction count + call count.

If any row in this script ever drifts from the published numbers,
that's a regression in either the recovered `phase17` INDEX-5
function or the reference executor's CC behaviour - the same
mechanism v0732g/v0734e audited. Both gates are proven able to fail
by planting the defect (the existing f3_individual_value_row test
fixture catches the negative).

This is a Python script (not a vf2probe wrapper) because it has to
parse JSON from two CLI tools and assert equality. The same pattern
as `test_block_coverage.py`.
"""

import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
BUILD = ROOT / "build" / "Debug"
VF2PROBE = BUILD / "vf2probe.exe"
VF2I960 = BUILD / "vf2i960.exe"
ROM_DIR = ROOT / "roms" / "vf2"

# Each leg: (snapshot, expected_instructions, expected_calls, description)
CASE = [
    ("indk-c.vf2snap", 4293, 38, "row-1 INDIVIDUAL KICK release"),
    ("indp-c.vf2snap", 4293, 38, "row-1 INDIVIDUAL PUNCH release"),
    ("indk2-c.vf2snap", 4294, 38, "row-2 INDIVIDUAL KICK release"),
]

STOP_ENTRY = "0x9ff8"
STOP_END = "0xa010"


def run_reference(snapshot: Path) -> dict:
    """Drive the reference (vf2probe) over the 0x9ff8 -> 0xa010 frame."""
    base_calls = None
    # First run: stop at 0x9ff8 to get the baseline counters.
    out = subprocess.run(
        [str(VF2PROBE), "--rom-dir", str(ROM_DIR), "--snapshot", str(snapshot),
         "--until", STOP_ENTRY],
        capture_output=True, text=True, timeout=600,
    )
    if out.returncode != 0:
        raise RuntimeError(f"reference baseline failed for {snapshot.name}: {out.stderr}")
    base = json.loads(out.stdout.splitlines()[-1])
    base_calls = base["procedure_calls"]

    # Second run: stop at 0xa010 to get the end counters.
    out = subprocess.run(
        [str(VF2PROBE), "--rom-dir", str(ROM_DIR), "--snapshot", str(snapshot),
         "--until", STOP_END],
        capture_output=True, text=True, timeout=600,
    )
    if out.returncode != 0:
        raise RuntimeError(f"reference end failed for {snapshot.name}: {out.stderr}")
    end = json.loads(out.stdout.splitlines()[-1])
    return {
        "instructions": end["run_instructions"],
        "calls": end["procedure_calls"] - base_calls,
    }


def run_native(snapshot: Path) -> dict:
    """Drive the native (vf2i960 native-resume) to 0xa010."""
    out = subprocess.run(
        [str(VF2I960), "native-resume", str(ROM_DIR), str(snapshot),
         "100000", "0", STOP_END],
        capture_output=True, text=True, timeout=600,
    )
    if out.returncode != 0:
        raise RuntimeError(f"native failed for {snapshot.name}: {out.stderr}")
    # The output is two lines:
    #   Native resume: blocks=1 instructions=4293 entry=0x00009ff8 exit=0x0000a010 task=none
    #     calls=38 returns=38 fighter_flags_or=0x00000000
    line1 = next(l for l in out.stdout.splitlines() if l.startswith("Native resume:"))
    line2 = next(l for l in out.stdout.splitlines() if "calls=" in l)
    parts1 = dict(p.split("=") for p in line1.split() if "=" in p)
    parts2 = dict(p.split("=") for p in line2.split() if "=" in p)
    return {
        "instructions": int(parts1["instructions"]),
        "calls": int(parts2["calls"]),
    }


def main() -> int:
    if not VF2PROBE.exists() or not VF2I960.exists():
        print(f"SKIP: missing tools ({VF2PROBE} / {VF2I960})", file=sys.stderr)
        return 0
    if not ROM_DIR.exists():
        print(f"SKIP: missing ROM directory ({ROM_DIR})", file=sys.stderr)
        return 0

    failures = 0
    for name, want_ins, want_calls, descr in CASE:
        path = ROOT / "out" / name
        if not path.exists():
            print(f"SKIP: {name} not present in {ROOT/'out'}", file=sys.stderr)
            continue
        print(f"-- {descr} ({name})")
        ref = run_reference(path)
        print(f"   reference: instructions={ref['instructions']} calls={ref['calls']}")
        nat = run_native(path)
        print(f"   native   : instructions={nat['instructions']} calls={nat['calls']}")

        ok = True
        if ref["instructions"] != nat["instructions"]:
            print(f"   FAIL: reference/native instruction mismatch "
                  f"({ref['instructions']} vs {nat['instructions']})")
            ok = False
        if ref["calls"] != nat["calls"]:
            print(f"   FAIL: reference/native call mismatch "
                  f"({ref['calls']} vs {nat['calls']})")
            ok = False
        if ref["instructions"] != want_ins or ref["calls"] != want_calls:
            print(f"   FAIL: reference does not match published "
                  f"({want_ins}/{want_calls})")
            ok = False
        if nat["instructions"] != want_ins or nat["calls"] != want_calls:
            print(f"   FAIL: native does not match published "
                  f"({want_ins}/{want_calls})")
            ok = False
        if ok:
            print(f"   ok: FULL MATCH ({want_ins}/{want_calls})")
        else:
            failures += 1

    if failures:
        print(f"\n{failures}/{len(CASE)} INDIVIDUAL releases FAIL")
        return 1
    print(f"\n{len(CASE)}/{len(CASE)} INDIVIDUAL releases FULL MATCH")
    return 0


if __name__ == "__main__":
    sys.exit(main())