#!/usr/bin/env python3
import argparse
import json
import subprocess
from pathlib import Path


def parse_int(value):
    if isinstance(value, int):
        return value
    return int(str(value), 0)


def cli_int(value):
    return f"-{abs(value):#x}" if value < 0 else f"{value:#x}"


def load_json(path):
    with open(path, "r", encoding="utf-8") as stream:
        return json.load(stream)


def dimension_values(dimension):
    if "values" in dimension:
        return [parse_int(value) for value in dimension["values"]]
    bits = [parse_int(bit) for bit in dimension.get("bits", [])]
    base = parse_int(dimension.get("base", 0))
    values = []
    for mask in range(1 << len(bits)):
        value = base
        for index, bit in enumerate(bits):
            if mask & (1 << index):
                value |= 1 << bit
        values.append(value)
    return values


def mutation_args(dimension, value):
    kind = dimension["kind"]
    if kind == "reg":
        return ["--set-reg", f"{dimension['register']}={cli_int(value)}"]
    return [
        f"--set-{kind}",
        f"{parse_int(dimension['address']):#x}={cli_int(value)}",
    ]


def reaches_edge(scenario, dimensions, values, edge):
    command = [
        str(scenario["probe"]),
        "--rom-dir",
        str(scenario["rom_dir"]),
        "--snapshot",
        str(scenario["snapshot"]),
        "--max-steps",
        str(parse_int(scenario.get("max_steps", 100000))),
        "--trace",
    ]
    if "until" in scenario:
        command += ["--until", f"{parse_int(scenario['until']):#x}"]
    for dimension, value in zip(dimensions, values):
        command += mutation_args(dimension, value)

    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        bufsize=1,
    )
    found = False
    assert process.stdout is not None
    for line in process.stdout:
        record = json.loads(line)
        if record.get("type") != "step":
            continue
        candidate = (int(record["ip_before"]), int(record["ip_after"]))
        if candidate == edge:
            found = True
    return process.wait() == 0 and found


def parse_edge(text):
    source_text, separator, target_text = text.partition(":")
    if not separator:
        raise ValueError("edge must be FROM:TO")
    return parse_int(source_text), parse_int(target_text)


def main():
    parser = argparse.ArgumentParser(
        description="Minimize a coverage-discovered vf2 state while preserving a target edge"
    )
    parser.add_argument("scenario")
    parser.add_argument("case")
    parser.add_argument("--edge", required=True, help="guest edge FROM:TO")
    parser.add_argument("--output")
    args = parser.parse_args()

    scenario = load_json(args.scenario)
    case = load_json(args.case)
    dimensions = scenario["dimensions"]
    names = [dimension["name"] for dimension in dimensions]
    current = [parse_int(case["inputs"][name]) for name in names]
    baselines = [dimension_values(dimension)[0] for dimension in dimensions]
    edge = parse_edge(args.edge)

    if not reaches_edge(scenario, dimensions, current, edge):
        raise SystemExit("the supplied case does not reproduce the requested edge")

    changed = True
    while changed:
        changed = False
        for index, dimension in enumerate(dimensions):
            if current[index] == baselines[index]:
                continue
            candidate = list(current)
            candidate[index] = baselines[index]
            if reaches_edge(scenario, dimensions, candidate, edge):
                current = candidate
                changed = True
                continue

            if "bits" not in dimension:
                continue
            base = parse_int(dimension.get("base", 0))
            for bit in sorted((parse_int(bit) for bit in dimension["bits"]), reverse=True):
                bit_mask = 1 << bit
                if (current[index] & bit_mask) == 0:
                    continue
                candidate = list(current)
                candidate[index] &= ~bit_mask
                candidate[index] |= base
                if reaches_edge(scenario, dimensions, candidate, edge):
                    current = candidate
                    changed = True

    result = {
        "target_edge": {"from": edge[0], "to": edge[1]},
        "inputs": dict(zip(names, current)),
        "original_inputs": case["inputs"],
    }
    text = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        Path(args.output).write_text(text, encoding="utf-8")
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
