#!/usr/bin/env python3
"""Unit tests for tools/python/infer_rules.py.

Runs standalone (no pytest required):

    python tools/python/test_infer_rules.py

Locks in the AGENTS.md documented contract:

> infer_rules.py is deliberately conservative. If selected features do
> not uniquely determine the outcome, or the truth table is incomplete,
> it should refuse to produce a minimized rule.

Coverage:

- stable_outcome signature is stable across inputs / returncode
- parse_bitfield validation accepts/ rejects edge cases
- feature_vector expansion handles boolean and bitfield inputs
- try_boolean_minimize refuses when the truth table is incomplete
- try_boolean_minimize refuses when the same feature vector yields
  two different outcomes (the "features do not fully determine"
  contract)
- try_boolean_minimize returns a minimized rule on a complete table
  when sympy is available
"""

from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from infer_rules import (
    feature_vector,
    load_records,
    parse_bitfield,
    stable_outcome,
    try_boolean_minimize,
)


def _record(inputs, status="ok", halt_reason="stop address", ip="0x10dcc",
            executed=10, calls=2, returns=2, reads_u32=None):
    rec = {
        "inputs": inputs,
        "outcome": {
            "status": status,
            "halt_reason": halt_reason,
            "ip": ip,
            "executed_instructions": executed,
            "procedure_calls": calls,
            "procedure_returns": returns,
            "reads_u32": reads_u32 or [],
        },
    }
    return rec


def test_stable_outcome_is_deterministic():
    """Same outcome fields produce the same tuple regardless of input order."""
    a = _record({"x": 1})
    b = _record({"x": 99})  # different inputs, same outcome fields
    assert stable_outcome(a) == stable_outcome(b), (
        stable_outcome(a), stable_outcome(b)
    )
    # Different executed count -> different signature
    c = _record({"x": 1}, executed=11)
    assert stable_outcome(a) != stable_outcome(c)
    print("ok: stable_outcome signature is deterministic")


def test_stable_outcome_handles_missing_outcome():
    """Records without an outcome collapse to a probe_failure key."""
    rec = {"returncode": 0}
    sig = stable_outcome(rec)
    assert sig[0] == "probe_failure", sig
    print("ok: stable_outcome collapses missing outcome to probe_failure")


def test_parse_bitfield_accepts_valid_specs():
    name, bits = parse_bitfield("flags:1,2,4,6,8")
    assert name == "flags"
    assert bits == [1, 2, 4, 6, 8]
    # single bit
    name, bits = parse_bitfield("flag:0")
    assert (name, bits) == ("flag", [0])
    print("ok: parse_bitfield accepts valid specs")


def test_parse_bitfield_rejects_invalid_specs():
    for spec in ["", "no_colon", ":1,2", "name:", "name:32", "name:-1",
                 "name:1,1", "name:abc"]:
        try:
            parse_bitfield(spec)
        except ValueError:
            continue
        raise AssertionError(f"parse_bitfield accepted {spec!r}")
    print("ok: parse_bitfield rejects malformed specs")


def test_feature_vector_boolean_and_bitfield():
    """Boolean + bitfield expansion into feature tuples."""
    rec = _record({"healthy": 1, "flags": 0b01010000})  # flags bits 4 + 6 set
    names, values = feature_vector(
        rec,
        boolean_names=["healthy"],
        bitfields=[("flags", [1, 2, 4, 6, 8])],
    )
    # healthy_b6 is the dominant bit; bit 4 too
    assert names == ("healthy",
              "flags_b1", "flags_b2", "flags_b4", "flags_b6", "flags_b8"), names
    assert values == (1, 0, 0, 1, 1, 0), values
    print("ok: feature_vector expands boolean + bitfield into named bits")


def test_feature_vector_rejects_non_boolean():
    rec = _record({"healthy": 2})
    try:
        feature_vector(rec, boolean_names=["healthy"], bitfields=[])
    except ValueError:
        pass
    else:
        raise AssertionError("expected ValueError on non-boolean input")
    print("ok: feature_vector rejects non-binary boolean inputs")


def test_minimize_refuses_incomplete_truth_table():
    """A partial sweep over a 2-bit domain must be refused."""
    records = [
        _record({"f0": 0, "f1": 0}),
        _record({"f0": 0, "f1": 1}),
        # missing f0=1 entries -> only 2 of 4 combinations present
    ]
    boolean = []
    bitfields = [("f0", [0]), ("f1", [0])]
    rule, reason = try_boolean_minimize(
        records, boolean, bitfields,
        target_outcome=stable_outcome(records[0]),
    )
    assert rule is None, rule
    assert reason is not None and "incomplete" in reason, reason
    print("ok: minimize refuses an incomplete truth table")


def test_minimize_refuses_non_determining_features():
    """Same feature vector mapping to two outcomes must be refused."""
    target = ("ok_a", "stop_a", 0x10dcc, 5, 1, 1, ())
    other = ("ok_b", "stop_b", 0x10ddc, 6, 1, 1, ())
    # Two records share the (f0=0, f1=0) feature vector but resolve to
    # different outcomes: the "hidden" f2 input is what disambiguates,
    # but try_boolean_minimize only sees f0/f1 so the features do not
    # fully determine the outcome.
    records = [
        {"inputs": {"f0": 0, "f1": 0, "f2": 0},
         "outcome": {
             "status": target[0], "halt_reason": target[1],
             "ip": target[2], "executed_instructions": target[3],
             "procedure_calls": target[4], "procedure_returns": target[5],
             "reads_u32": [],
         }},
        {"inputs": {"f0": 0, "f1": 0, "f2": 1},
         "outcome": {
             "status": other[0], "halt_reason": other[1],
             "ip": other[2], "executed_instructions": other[3],
             "procedure_calls": other[4], "procedure_returns": other[5],
             "reads_u32": [],
         }},
        # Fill the rest of the 4-combo truth table with the target
        # outcome so the truth table is complete on (f0, f1).
        {"inputs": {"f0": 0, "f1": 1, "f2": 0},
         "outcome": {
             "status": target[0], "halt_reason": target[1],
             "ip": target[2], "executed_instructions": target[3],
             "procedure_calls": target[4], "procedure_returns": target[5],
             "reads_u32": [],
         }},
        {"inputs": {"f0": 1, "f1": 0, "f2": 0},
         "outcome": {
             "status": target[0], "halt_reason": target[1],
             "ip": target[2], "executed_instructions": target[3],
             "procedure_calls": target[4], "procedure_returns": target[5],
             "reads_u32": [],
         }},
        {"inputs": {"f0": 1, "f1": 1, "f2": 0},
         "outcome": {
             "status": target[0], "halt_reason": target[1],
             "ip": target[2], "executed_instructions": target[3],
             "procedure_calls": target[4], "procedure_returns": target[5],
             "reads_u32": [],
         }},
    ]
    boolean = []
    bitfields = [("f0", [0]), ("f1", [0])]
    rule, reason = try_boolean_minimize(
        records, boolean, bitfields, target_outcome=target,
    )
    assert rule is None, rule
    assert reason is not None and "do not fully determine" in reason, reason
    print("ok: minimize refuses when features do not determine the outcome")


def test_minimize_handles_complete_table_when_sympy_missing_or_present():
    """A complete table where the rule is trivial must be handled.

    The contract: when sympy is missing, try_boolean_minimize returns
    ``(None, reason)`` with a clear message. When sympy is present, it
    returns a minimized rule string.
    """
    target = ("ok_a", "stop_a", 0x10dcc, 5, 1, 1, ())
    other = ("ok_b", "stop_b", 0x10ddc, 6, 1, 1, ())
    records = []
    for f0 in (0, 1):
        for f1 in (0, 1):
            chosen = target if (f0, f1) in {(0, 0), (1, 1)} else other
            records.append({
                "inputs": {"f0": f0, "f1": f1},
                "outcome": {
                    "status": chosen[0], "halt_reason": chosen[1],
                    "ip": chosen[2], "executed_instructions": chosen[3],
                    "procedure_calls": chosen[4], "procedure_returns": chosen[5],
                    "reads_u32": [],
                },
            })
    boolean = []
    bitfields = [("f0", [0]), ("f1", [0])]
    rule, reason = try_boolean_minimize(
        records, boolean, bitfields, target_outcome=target,
    )
    try:
        import sympy  # noqa: F401
        assert rule is not None, f"sympy available but no rule returned: {reason}"
        # On f0 == f1, the rule is f0 & f1 | ~f0 & ~f1 (XOR negated).
        # The simplifier may render this in CNF/DNF form; accept any
        # non-empty string that mentions both bits.
        assert "f0" in rule and "f1" in rule, rule
        print(f"ok: minimize produced rule {rule!r} (sympy available)")
    except ImportError:
        assert rule is None, f"sympy missing but rule returned: {rule}"
        assert reason is not None, "expected reason when sympy is missing"
        assert "sympy" in reason, reason
        print("ok: minimize reports sympy-missing cleanly (no rule)")


def test_load_records_skips_blank_lines():
    """Blank lines in the JSONL must not break load_records."""
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "sweep.jsonl"
        path.write_text(
            json.dumps(_record({"f0": 0})) + "\n\n"
            + json.dumps(_record({"f0": 1})) + "\n",
            encoding="utf-8",
        )
        records = list(load_records(path))
    assert len(records) == 2, records
    print("ok: load_records tolerates blank lines")


def main() -> int:
    test_stable_outcome_is_deterministic()
    test_stable_outcome_handles_missing_outcome()
    test_parse_bitfield_accepts_valid_specs()
    test_parse_bitfield_rejects_invalid_specs()
    test_feature_vector_boolean_and_bitfield()
    test_feature_vector_rejects_non_boolean()
    test_minimize_refuses_incomplete_truth_table()
    test_minimize_refuses_non_determining_features()
    test_minimize_handles_complete_table_when_sympy_missing_or_present()
    test_load_records_skips_blank_lines()
    print("all infer_rules tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())