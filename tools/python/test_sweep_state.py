#!/usr/bin/env python3
"""Unit tests for tools/python/sweep_state.py.

Runs standalone (no pytest required):

    python tools/python/test_sweep_state.py

Validates the pure-Python helpers (scenario loading, dimension value
expansion, mutation-args construction). The vf2probe subprocess
invocation is intentionally NOT exercised; it requires a ROM and a
checkpoint, both of which the existing ROM-backed differential paths
already cover end-to-end.
"""

from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from sweep_state import (
    cli_int,
    dimension_values,
    load_scenario,
    mutation_args,
    parse_int,
)


def test_cli_int_formats_signed_hex():
    """cli_int converts signed Python ints into hex-with-sign strings."""
    assert cli_int(0) == "0x0"
    assert cli_int(255) == "0xff"
    assert cli_int(-1) == "-0x1"
    assert cli_int(-4096) == "-0x1000"
    print("ok: cli_int formats signed hex correctly")


def test_load_scenario_requires_required_fields():
    """The scenario must declare probe, rom_dir, snapshot, dimensions."""
    base = {
        "probe": "build/Debug/vf2probe.exe",
        "rom_dir": "roms/vf2",
        "snapshot": "out/sixth-fresh.vf2snap",
        "dimensions": [
            {"name": "x", "kind": "u8", "address": "0x10", "values": [0, 1]}
        ],
    }
    with tempfile.TemporaryDirectory() as tmp:
        for missing in ("probe", "rom_dir", "snapshot", "dimensions"):
            data = {k: v for k, v in base.items() if k != missing}
            if missing == "dimensions":
                data["dimensions"] = "not a list"
            path = Path(tmp) / "scenario.json"
            path.write_text(json.dumps(data), encoding="utf-8")
            try:
                load_scenario(path)
            except ValueError:
                continue
            raise AssertionError(f"load_scenario accepted scenario missing {missing!r}")
    print("ok: load_scenario rejects scenarios missing required fields")


def test_dimension_values_expands_values_list():
    """A dimension with a literal values list returns those values in order."""
    dim = {"name": "x", "kind": "u8", "address": "0x10", "values": [0, 1, 2]}
    assert dimension_values(dim) == [0, 1, 2], dimension_values(dim)
    print("ok: dimension_values expands a values list")


def test_dimension_values_expands_bits_with_base():
    """Bits expand into 2^|bits| combinations OR'd onto the base mask."""
    dim = {"name": "f", "kind": "u8", "address": "0x10",
           "bits": [1, 2], "base": 0x80}
    values = dimension_values(dim)
    assert len(values) == 4, values
    # 0x80 alone, 0x82, 0x84, 0x86
    assert sorted(values) == [0x80, 0x82, 0x84, 0x86], values
    print("ok: dimension_values expands bits onto base")


def test_dimension_values_rejects_no_values_or_bits():
    """A dimension with neither values nor bits is rejected."""
    dim = {"name": "x", "kind": "u8", "address": "0x10"}
    try:
        dimension_values(dim)
    except ValueError:
        pass
    else:
        raise AssertionError("dimension_values accepted a dimension with neither values nor bits")
    print("ok: dimension_values rejects dimensions with neither values nor bits")


def test_mutation_args_for_reg_dimension():
    """A reg dimension emits --set-reg <reg>=<hex>."""
    dim = {"name": "g0_step", "kind": "reg", "register": "g0"}
    args = mutation_args(dim, 5)
    assert args == ["--set-reg", "g0=0x5"], args
    args_neg = mutation_args(dim, -1)
    assert args_neg == ["--set-reg", "g0=-0x1"], args_neg
    print("ok: mutation_args handles reg dimensions (positive and negative values)")


def test_mutation_args_for_memory_dimension():
    """A memory dimension emits --set-<kind> <addr>=<hex>."""
    for kind in ("u8", "u16", "u32"):
        dim = {"name": "x", "kind": kind, "address": "0x510b24"}
        args = mutation_args(dim, 0x40)
        assert args == [f"--set-{kind}", "0x510b24=0x40"], (kind, args)
    print("ok: mutation_args handles u8/u16/u32 memory dimensions")


def test_mutation_args_rejects_unknown_kind():
    """An unknown dimension kind raises ValueError."""
    dim = {"name": "x", "kind": "u64", "address": "0x10"}
    try:
        mutation_args(dim, 0)
    except ValueError:
        pass
    else:
        raise AssertionError("mutation_args accepted u64 dimension")
    print("ok: mutation_args rejects unsupported dimension kinds")


def test_parse_int_handles_int_and_string():
    """parse_int accepts both int and stringified-int (with 0x prefix)."""
    assert parse_int(0x10) == 0x10
    assert parse_int("0x10") == 0x10
    assert parse_int("16") == 16
    assert parse_int(0) == 0
    print("ok: parse_int accepts both int and string forms")


def main() -> int:
    test_cli_int_formats_signed_hex()
    test_load_scenario_requires_required_fields()
    test_dimension_values_expands_values_list()
    test_dimension_values_expands_bits_with_base()
    test_dimension_values_rejects_no_values_or_bits()
    test_mutation_args_for_reg_dimension()
    test_mutation_args_for_memory_dimension()
    test_mutation_args_rejects_unknown_kind()
    test_parse_int_handles_int_and_string()
    print("all sweep_state tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())