#!/usr/bin/env python3
"""Unit tests for tools/python/z3_branch.py.

Runs standalone (no pytest required) when z3 is installed:

    python tools/python/test_z3_branch.py

Skips gracefully if z3-solver is not installed:

    skipped: z3-solver not installed; install with `pip install z3-solver`

Validates:

- parse_bits parses comma-separated bit positions
- parse_bits tolerates surrounding whitespace and empty fragments
- prove_equivalence confirms equivalent bit-mask predicates (unsat)
- prove_equivalence confirms inequivalent predicates (sat)
- prove_equivalence reports when extra_constraints make the
  predicates equivalent only on the constrained domain
"""

from __future__ import annotations

import sys

try:
    import z3
except ImportError:
    print("skip: z3-solver not installed; install with `pip install z3-solver`")
    sys.exit(0)

sys.path.insert(0, str(__file__).rsplit("/", 2)[0] + "/python")

from z3_branch import bv, parse_bits, prove_equivalence


def test_parse_bits_basic():
    """parse_bits splits a comma-separated string into ints."""
    assert parse_bits("1,2,4,6,8") == [1, 2, 4, 6, 8]
    assert parse_bits("0") == [0]
    assert parse_bits("31") == [31]
    print("ok: parse_bits splits comma-separated bit positions")


def test_parse_bits_tolerates_whitespace():
    """parse_bits skips empty fragments and accepts whitespace."""
    assert parse_bits("1, 2, 4") == [1, 2, 4]
    assert parse_bits(" 1 , 2 , 4 ") == [1, 2, 4]
    assert parse_bits("") == []
    assert parse_bits(",,") == []
    assert parse_bits("1,,2") == [1, 2]
    print("ok: parse_bits tolerates whitespace and empty fragments")


def test_prove_equivalence_accepts_equal_predicates():
    """prove_equivalence returns (True, None) when expr1 and expr2 are equal."""
    f = bv("f")
    # "f & 0x10 == 0" is the same as "(f >> 4) & 1 == 0"
    expr1 = (f & 0x10) == 0
    expr2 = ((f >> 4) & 1) == 0
    ok, model = prove_equivalence(expr1, expr2)
    assert ok is True and model is None, (ok, model)
    print("ok: prove_equivalence accepts equivalent predicates")


def test_prove_equivalence_rejects_inequal_predicates():
    """prove_equivalence returns (False, model) when expr1 and expr2 differ."""
    f = bv("f")
    # "f & 0x10 == 0" is NOT the same as "f & 0x20 == 0" generally
    expr1 = (f & 0x10) == 0
    expr2 = (f & 0x20) == 0
    ok, model = prove_equivalence(expr1, expr2)
    assert ok is False and model is not None, (ok, model)
    print("ok: prove_equivalence rejects inequivalent predicates")


def test_prove_equivalence_respects_extra_constraints():
    """When extra_constraints restrict the domain, predicates may agree."""
    f = bv("f")
    # Force bits 4 AND 5 to 0; then "f & 0x10 == 0" is equivalent
    # to "f & 0x20 == 0" because both mask bits are 0.
    constraint = (f & 0x30) == 0
    expr1 = (f & 0x10) == 0
    expr2 = (f & 0x20) == 0
    ok, model = prove_equivalence(expr1, expr2, [constraint])
    assert ok is True and model is None, (ok, model)
    print("ok: prove_equivalence respects extra_constraints")


def main() -> int:
    test_parse_bits_basic()
    test_parse_bits_tolerates_whitespace()
    test_prove_equivalence_accepts_equal_predicates()
    test_prove_equivalence_rejects_inequal_predicates()
    test_prove_equivalence_respects_extra_constraints()
    print("all z3_branch tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())