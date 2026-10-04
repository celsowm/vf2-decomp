#!/usr/bin/env python3
"""Unit tests for tools/python/check_scenario.py.

Runs standalone (no pytest required):

    python tools/python/test_check_scenario.py

Locks in the scenario-validation contract that gates every sweep:
a malformed scenario must be rejected with a clear message, and the
generated-cases count must match the cartesian product of the
dimension value sets.
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECK_SCRIPT = ROOT / "tools" / "python" / "check_scenario.py"


def _scenario_dict(**kwargs) -> dict:
    """Build a minimal valid scenario and override fields."""
    base = {
        "probe": "build/Debug/vf2probe.exe",
        "rom_dir": "roms/vf2",
        "snapshot": "out/sixth-fresh.vf2snap",
        "dimensions": [
            {"name": "fighter0_flags", "kind": "u8",
             "address": "0x00510b24", "values": [0, 1, 2]},
        ],
    }
    base.update(kwargs)
    return base


def _run_check(scenario_path: Path):
    """Run check_scenario.py against the scenario file, capture rc/stdout."""
    proc = subprocess.run(
        [sys.executable, str(CHECK_SCRIPT), str(scenario_path)],
        capture_output=True, text=True,
    )
    return proc.returncode, proc.stdout, proc.stderr


def test_valid_scenario_is_accepted():
    """A minimal valid scenario prints the case count and exits 0."""
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "scenario.json"
        path.write_text(json.dumps(_scenario_dict()), encoding="utf-8")
        rc, out, err = _run_check(path)
    assert rc == 0, (rc, err)
    assert "1 dimensions" in out
    assert "3 generated cases" in out
    print("ok: valid scenario accepted with correct case count")


def test_missing_required_field_rejected():
    """Required fields are probe, rom_dir, snapshot, dimensions."""
    for missing in ("probe", "rom_dir", "snapshot", "dimensions"):
        with tempfile.TemporaryDirectory() as tmp:
            data = _scenario_dict()
            del data[missing]
            path = Path(tmp) / "scenario.json"
            path.write_text(json.dumps(data), encoding="utf-8")
            rc, out, err = _run_check(path)
        assert rc != 0, (missing, rc)
        assert "missing required field" in err, (missing, err)
    print("ok: missing required fields rejected")


def test_invalid_dimension_kind_rejected():
    """Only reg/u8/u16/u32 dimension kinds are allowed."""
    with tempfile.TemporaryDirectory() as tmp:
        data = _scenario_dict()
        data["dimensions"][0]["kind"] = "u64"
        path = Path(tmp) / "scenario.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        rc, out, err = _run_check(path)
    assert rc != 0
    assert "unsupported dimension kind" in err, err
    print("ok: invalid dimension kind rejected")


def test_register_dimension_requires_register_field():
    """A 'reg' dimension must name a register."""
    with tempfile.TemporaryDirectory() as tmp:
        data = _scenario_dict()
        data["dimensions"][0] = {
            "name": "g0_step", "kind": "reg",
            "values": [0, 1, 2],
            # missing "register" key
        }
        path = Path(tmp) / "scenario.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        rc, out, err = _run_check(path)
    assert rc != 0
    assert "requires register" in err, err
    print("ok: register dimension without register name rejected")


def test_dimension_must_have_exactly_one_of_values_or_bits():
    """A dimension cannot specify both 'values' and 'bits' or neither."""
    # Both 'values' and 'bits' present
    with tempfile.TemporaryDirectory() as tmp:
        data = _scenario_dict()
        data["dimensions"][0]["bits"] = [0, 1]
        path = Path(tmp) / "scenario.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        rc, out, err = _run_check(path)
    assert rc != 0
    assert "exactly one of values or bits" in err, err
    # Neither 'values' nor 'bits' present
    with tempfile.TemporaryDirectory() as tmp:
        data = _scenario_dict()
        data["dimensions"][0].pop("values")
        path = Path(tmp) / "scenario.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        rc, out, err = _run_check(path)
    assert rc != 0
    assert "exactly one of values or bits" in err, err
    print("ok: dimensions must have exactly one of values or bits")


def test_values_must_be_non_empty_and_32bit():
    """An empty values list or out-of-range value is rejected."""
    with tempfile.TemporaryDirectory() as tmp:
        data = _scenario_dict()
        data["dimensions"][0]["values"] = []
        path = Path(tmp) / "scenario.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        rc, out, err = _run_check(path)
    assert rc != 0 and "no values" in err, err
    # Out-of-range value
    with tempfile.TemporaryDirectory() as tmp:
        data = _scenario_dict()
        data["dimensions"][0]["values"] = [0, 0xFFFFFFFF + 1]
        path = Path(tmp) / "scenario.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        rc, out, err = _run_check(path)
    assert rc != 0 and "outside 32 bits" in err, err
    print("ok: values must be non-empty and within 32 bits")


def test_bit_positions_must_be_unique_and_in_range():
    """Bits must be 0..31 with no duplicates; base must not overlap."""
    for bad in (
        {"bits": [0, 0]},      # duplicate
        {"bits": [-1]},          # negative
        {"bits": [32]},          # out of range
    ):
        with tempfile.TemporaryDirectory() as tmp:
            data = _scenario_dict()
            data["dimensions"][0].pop("values", None)
            data["dimensions"][0].update(bad)
            path = Path(tmp) / "scenario.json"
            path.write_text(json.dumps(data), encoding="utf-8")
            rc, out, err = _run_check(path)
        assert rc != 0, (bad, rc)
        assert "invalid/repeated bit positions" in err, (bad, err)
    # Base overlap with swept bits
    with tempfile.TemporaryDirectory() as tmp:
        data = _scenario_dict()
        data["dimensions"][0].pop("values", None)
        data["dimensions"][0]["bits"] = [4, 6]
        data["dimensions"][0]["base"] = 0x10  # bit 4 overlaps
        path = Path(tmp) / "scenario.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        rc, out, err = _run_check(path)
    assert rc != 0 and "base overlaps" in err, err
    print("ok: bit positions validated and base-overlap rejected")


def test_case_count_is_cartesian_product():
    """Total = product of len(values) across dimensions (or 2^|bits|)."""
    data = _scenario_dict(dimensions=[
        {"name": "a", "kind": "u8", "address": "0x10", "values": [0, 1, 2]},
        {"name": "b", "kind": "u8", "address": "0x11", "values": [0, 1]},
        {"name": "c", "kind": "u32", "address": "0x12", "bits": [1, 2, 4, 6]},
    ])
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "scenario.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        rc, out, err = _run_check(path)
    assert rc == 0
    assert "3 dimensions" in out
    # 3 * 2 * 2^4 = 96 (values dims x values dims x bits dim)
    assert "96 generated cases" in out, out
    print("ok: case count = cartesian product across dimensions")


def test_until_address_must_be_32bit():
    """Optional 'until' field must be a valid 32-bit address."""
    for bad in ("-1", "0x100000000"):
        with tempfile.TemporaryDirectory() as tmp:
            data = _scenario_dict(until=bad)
            path = Path(tmp) / "scenario.json"
            path.write_text(json.dumps(data), encoding="utf-8")
            rc, out, err = _run_check(path)
        assert rc != 0, (bad, rc)
        assert "until" in err, err
    print("ok: 'until' address must be 32-bit")


def main() -> int:
    test_valid_scenario_is_accepted()
    test_missing_required_field_rejected()
    test_invalid_dimension_kind_rejected()
    test_register_dimension_requires_register_field()
    test_dimension_must_have_exactly_one_of_values_or_bits()
    test_values_must_be_non_empty_and_32bit()
    test_bit_positions_must_be_unique_and_in_range()
    test_case_count_is_cartesian_product()
    test_until_address_must_be_32bit()
    print("all check_scenario tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())