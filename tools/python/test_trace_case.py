#!/usr/bin/env python3
"""Unit tests for tools/python/trace_case.py.

Runs standalone (no pytest required):

    python tools/python/test_trace_case.py

Validates the pure-Python helpers of the trace_case tool:

- parse_override parses 'NAME=VALUE' into a (name, int) tuple.
- parse_override rejects strings without '=' separator.
- parse_override rejects empty name.
- load_scenario requires probe / rom_dir / snapshot / dimensions.
"""

from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from trace_case import load_scenario, parse_override


def test_parse_override_accepts_hex_value():
    """parse_override parses 'NAME=0xVALUE' into a (name, int) tuple."""
    name, value = parse_override("fighter0_flags=0x40")
    assert name == "fighter0_flags", name
    assert value == 0x40, value
    # Decimal
    name, value = parse_override("count=10")
    assert name == "count" and value == 10, (name, value)
    print("ok: parse_override accepts hex and decimal values")


def test_parse_override_rejects_missing_equals():
    """parse_override raises ArgumentTypeError without '='."""
    try:
        parse_override("no-equals-here")
    except argparse.ArgumentTypeError:
        pass
    else:
        raise AssertionError("parse_override accepted a string without '='")
    print("ok: parse_override rejects strings without '=' separator")


def test_parse_override_rejects_empty_name():
    """parse_override raises ArgumentTypeError when the name half is empty."""
    try:
        parse_override("=0x40")
    except argparse.ArgumentTypeError:
        pass
    else:
        raise AssertionError("parse_override accepted an empty name")
    print("ok: parse_override rejects empty name")


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
            path = Path(tmp) / "scenario.json"
            path.write_text(json.dumps(data), encoding="utf-8")
            try:
                load_scenario(path)
            except ValueError:
                continue
            raise AssertionError(f"load_scenario accepted scenario missing {missing!r}")
    print("ok: load_scenario rejects scenarios missing required fields")


def main() -> int:
    test_parse_override_accepts_hex_value()
    test_parse_override_rejects_missing_equals()
    test_parse_override_rejects_empty_name()
    test_load_scenario_requires_required_fields()
    print("all trace_case tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())