#!/usr/bin/env python3
import argparse
import itertools
import json
from collections import Counter, defaultdict


def stable_outcome(record):
    outcome = record.get("outcome")
    if not outcome:
        return ("probe_failure", record.get("returncode"))
    reads = tuple(
        sorted(
            (item.get("address"), item.get("value"), item.get("error"))
            for item in outcome.get("reads_u32", [])
        )
    )
    return (
        outcome.get("status"),
        outcome.get("halt_reason"),
        outcome.get("ip"),
        outcome.get("executed_instructions"),
        outcome.get("procedure_calls"),
        outcome.get("procedure_returns"),
        reads,
    )


def load_records(path):
    with open(path, "r", encoding="utf-8") as stream:
        for line in stream:
            if line.strip():
                yield json.loads(line)


def parse_bitfield(spec):
    name, separator, bit_text = spec.partition(":")
    if not separator or not name or not bit_text:
        raise ValueError(f"invalid bitfield specification: {spec!r}")
    bits = [int(value, 0) for value in bit_text.split(",")]
    if len(set(bits)) != len(bits) or any(bit < 0 or bit > 31 for bit in bits):
        raise ValueError(f"invalid bit positions in: {spec!r}")
    return name, bits


def feature_vector(record, boolean_names, bitfields):
    inputs = record.get("inputs", {})
    names = []
    values = []

    for name in boolean_names:
        value = inputs.get(name)
        if value not in (0, 1):
            raise ValueError(f"{name!r} is not binary in every case")
        names.append(name)
        values.append(value)

    for field_name, bits in bitfields:
        if field_name not in inputs:
            raise ValueError(f"missing bitfield input {field_name!r}")
        field_value = int(inputs[field_name])
        for bit in bits:
            names.append(f"{field_name}_b{bit}")
            values.append(1 if field_value & (1 << bit) else 0)

    return tuple(names), tuple(values)


def try_boolean_minimize(records, boolean_names, bitfields, target_outcome):
    try:
        from sympy import symbols
        from sympy.logic import SOPform, simplify_logic
    except ImportError:
        return None, "sympy is not installed"

    if not boolean_names and not bitfields:
        return None, None

    vectors = {}
    feature_names = None
    try:
        for record in records:
            names, vector = feature_vector(record, boolean_names, bitfields)
            feature_names = names
            outcome = stable_outcome(record)
            previous = vectors.get(vector)
            if previous is not None and previous != outcome:
                return None, "selected boolean features do not fully determine the outcome"
            vectors[vector] = outcome
    except ValueError as error:
        return None, str(error)

    if feature_names is None:
        return None, "no cases"

    expected = 1 << len(feature_names)
    if len(vectors) != expected:
        return None, f"truth table is incomplete ({len(vectors)}/{expected} feature vectors)"

    variables = symbols(" ".join(feature_names))
    if len(feature_names) == 1:
        variables = (variables,)

    minterms = [list(vector) for vector, outcome in vectors.items() if outcome == target_outcome]
    if not minterms:
        return None, None
    if len(minterms) == expected:
        return "True", None

    expression = SOPform(variables, minterms)
    return str(simplify_logic(expression, form="dnf", force=True)), None


def main():
    parser = argparse.ArgumentParser(
        description="Group vf2 sweep outcomes and infer measured boolean rules"
    )
    parser.add_argument("input")
    parser.add_argument(
        "--boolean",
        nargs="*",
        default=[],
        help="dimension names whose values are explicitly 0/1",
    )
    parser.add_argument(
        "--bitfield",
        action="append",
        default=[],
        metavar="NAME:BITS",
        help="expand an integer input into bit features, e.g. flags:1,2,4,6,8",
    )
    args = parser.parse_args()

    try:
        bitfields = [parse_bitfield(spec) for spec in args.bitfield]
    except ValueError as error:
        parser.error(str(error))

    records = list(load_records(args.input))
    groups = defaultdict(list)
    for record in records:
        groups[stable_outcome(record)].append(record)

    print(f"cases: {len(records)}")
    print(f"distinct outcomes: {len(groups)}")
    print()

    ranked = sorted(groups.items(), key=lambda item: (-len(item[1]), repr(item[0])))
    reported_reason = None
    for index, (outcome, members) in enumerate(ranked, 1):
        print(f"outcome {index}: {len(members)} cases")
        print(f"  signature: {outcome}")
        if members:
            print(f"  example inputs: {members[0].get('inputs', {})}")
        rule, reason = try_boolean_minimize(records, args.boolean, bitfields, outcome)
        if rule is not None:
            print(f"  minimized rule: {rule}")
        if reason is not None:
            reported_reason = reason
        print()

    if reported_reason is not None:
        print(f"boolean minimization unavailable: {reported_reason}")
        print()

    failures = Counter(
        record.get("returncode") for record in records if record.get("returncode")
    )
    if failures:
        print("probe failures:")
        for code, count in sorted(failures.items()):
            print(f"  returncode {code}: {count}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
