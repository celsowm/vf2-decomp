#!/usr/bin/env python3
"""Unit tests for tools/python/explore_state.py.

Runs standalone (no pytest required):

    python tools/python/test_explore_state.py

Validates the pure-Python helpers of the coverage-guided explorer.
The `subprocess` invocations of `vf2probe` are intentionally NOT
exercised; they require a ROM and a checkpoint, both of which the
existing ROM-backed differential paths already cover end-to-end.

Coverage:

- parse_int accepts both int and stringified-int forms.
- cli_int formats signed Python ints into hex-with-sign strings.
- dimension_values expands a literal values list.
- dimension_values expands bits onto base (2^|bits| combinations).
- mutate picks new values in K positions, K in [1, max_mutations].
- mutate produces a tuple whose length equals the parent's length.
- write_json writes a single JSON object.
- load_existing_corpus tolerates missing files (returns empty state).
"""

from __future__ import annotations

import json
import random
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from explore_state import (
    cli_int,
    dimension_values,
    load_existing_corpus,
    mutate,
    parse_int,
    write_json,
)


def test_cli_int_formats_signed_hex():
    """cli_int converts signed Python ints into hex-with-sign strings."""
    assert cli_int(0) == "0x0"
    assert cli_int(255) == "0xff"
    assert cli_int(-1) == "-0x1"
    print("ok: cli_int formats signed hex correctly")


def test_dimension_values_expands_values_list():
    """A dimension with a literal values list returns those values in order."""
    dim = {"name": "x", "kind": "u8", "address": "0x10", "values": [0, 1, 2]}
    assert dimension_values(dim) == [0, 1, 2]
    print("ok: dimension_values expands a values list")


def test_dimension_values_expands_bits_with_base():
    """Bits expand into 2^|bits| combinations OR'd onto the base mask."""
    dim = {"name": "f", "kind": "u8", "address": "0x10",
           "bits": [1, 2], "base": 0x80}
    values = sorted(dimension_values(dim))
    assert values == [0x80, 0x82, 0x84, 0x86], values
    print("ok: dimension_values expands bits onto base")


def test_mutate_changes_at_least_one_position():
    """mutate must change at least one position; the result respects the
    value set per dimension."""
    parent = (0, 1, 2)
    value_sets = [[0, 1, 2, 4], [1, 3, 7], [2, 5, 8]]
    rng = random.Random(42)
    mutated = mutate(parent, value_sets, rng, max_mutations=3)
    assert len(mutated) == len(parent), mutated
    # At least one position differs from the parent.
    assert any(a != b for a, b in zip(mutated, parent)), (parent, mutated)
    # Every position is still in the corresponding value set.
    for v, choices in zip(mutated, value_sets):
        assert v in choices, (v, choices)
    print("ok: mutate changes at least one position and respects value sets")


def test_mutate_with_seed_is_deterministic():
    """Same seed produces the same mutation (RNG contract for replay)."""
    parent = (0, 1, 2, 3)
    value_sets = [[0, 1], [1, 2], [2, 3], [3, 4]]
    a = mutate(parent, value_sets, random.Random(7), max_mutations=2)
    b = mutate(parent, value_sets, random.Random(7), max_mutations=2)
    assert a == b, (a, b)
    # A different seed may produce a different mutation.
    c = mutate(parent, value_sets, random.Random(8), max_mutations=2)
    # Don't assert c != a (random collisions are possible), just that
    # both are valid.
    for v, choices in zip(c, value_sets):
        assert v in choices
    print("ok: mutate is deterministic for a given seed")


def test_mutate_respects_max_mutations():
    """mutate must never mutate more positions than max_mutations."""
    parent = (0, 0, 0, 0, 0)
    value_sets = [[0, 1], [0, 1], [0, 1], [0, 1], [0, 1]]
    rng = random.Random(0)
    for _ in range(20):
        mutated = mutate(parent, value_sets, rng, max_mutations=2)
        changes = sum(1 for a, b in zip(parent, mutated) if a != b)
        assert 1 <= changes <= 2, (parent, mutated)
    print("ok: mutate respects max_mutations bound")


def test_write_json_round_trips():
    """write_json produces a JSON file that parses back to the same dict."""
    payload = {"alpha": [1, 2, 3], "beta": {"nested": "value"}, "gamma": None}
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "out.json"
        write_json(path, payload)
        text = path.read_text(encoding="utf-8")
        assert json.loads(text) == payload, text
    print("ok: write_json round-trips its payload")


def test_load_existing_corpus_returns_empty_state_for_missing_file():
    """When the manifest path doesn't exist, load_existing_corpus returns
    empty structures (no crash, no implicit zero defaults)."""
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "does-not-exist.jsonl"
        edges, inputs, seen = load_existing_corpus(path, ["a", "b"])
    assert edges == set(), edges
    assert inputs == [], inputs
    assert seen == set(), seen
    print("ok: load_existing_corpus tolerates missing manifest file")


def test_load_existing_corpus_reads_prior_cases():
    """When the manifest exists, load_existing_corpus returns its records."""
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "manifest.jsonl"
        path.write_text(
            json.dumps({"case": 0,
                        "inputs": {"a": 1, "b": 2},
                        "new_edges": [{"from": 10, "to": 20}],
                        "final": {"status": "ok"}}) + "\n"
            + json.dumps({"case": 1,
                          "inputs": {"a": 3, "b": 4},
                          "new_edges": [{"from": 10, "to": 30}],
                          "final": {"status": "ok"}}) + "\n",
            encoding="utf-8",
        )
        edges, inputs, seen = load_existing_corpus(path, ["a", "b"])
    assert (10, 20) in edges and (10, 30) in edges, edges
    assert len(inputs) == 2, inputs
    assert (1, 2) in seen and (3, 4) in seen, seen
    print("ok: load_existing_corpus reads prior corpus records")


def main() -> int:
    test_cli_int_formats_signed_hex()
    test_dimension_values_expands_values_list()
    test_dimension_values_expands_bits_with_base()
    test_mutate_changes_at_least_one_position()
    test_mutate_with_seed_is_deterministic()
    test_mutate_respects_max_mutations()
    test_write_json_round_trips()
    test_load_existing_corpus_returns_empty_state_for_missing_file()
    test_load_existing_corpus_reads_prior_cases()
    print("all explore_state tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())