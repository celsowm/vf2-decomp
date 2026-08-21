#!/usr/bin/env python3
import argparse
import json
from collections import Counter, defaultdict


def stable_outcome(record):
    outcome = record.get("outcome")
    if not outcome:
        return ("probe_failure", record.get("returncode"))
    reads = tuple(sorted((item["address"], item["value"]) for item in outcome.get("reads_u32", [])))
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


def try_boolean_minimize(records, input_names, target_outcome):
    try:
        from sympy import symbols
        from sympy.logic import SOPform, simplify_logic
    except ImportError:
        return None

    if not input_names:
        return None
    for record in records:
        values = record.get("inputs", {})
        if any(values.get(name) not in (0, 1) for name in input_names):
            return None

    vars_ = symbols(" ".join(input_names))
    if len(input_names) == 1:
        vars_ = (vars_,)
    minterms = []
    for record in records:
        if stable_outcome(record) == target_outcome:
            minterms.append([record["inputs"][name] for name in input_names])
    if not minterms:
        return None
    return str(simplify_logic(SOPform(vars_, minterms), form="dnf", force=True))


def main():
    parser = argparse.ArgumentParser(description="Group vf2 sweep outcomes and infer simple boolean rules")
    parser.add_argument("input")
    parser.add_argument("--boolean", nargs="*", default=[], help="binary dimension names to minimize")
    args = parser.parse_args()

    records = list(load_records(args.input))
    groups = defaultdict(list)
    for record in records:
        groups[stable_outcome(record)].append(record)

    print(f"cases: {len(records)}")
    print(f"distinct outcomes: {len(groups)}")
    print()

    ranked = sorted(groups.items(), key=lambda item: (-len(item[1]), repr(item[0])))
    for index, (outcome, members) in enumerate(ranked, 1):
        print(f"outcome {index}: {len(members)} cases")
        print(f"  signature: {outcome}")
        if members:
            print(f"  example inputs: {members[0].get('inputs', {})}")
        rule = try_boolean_minimize(records, args.boolean, outcome)
        if rule is not None:
            print(f"  minimized rule: {rule}")
        print()

    failures = Counter(record.get("returncode") for record in records if record.get("returncode"))
    if failures:
        print("probe failures:")
        for code, count in sorted(failures.items()):
            print(f"  returncode {code}: {count}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
