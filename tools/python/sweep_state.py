#!/usr/bin/env python3
import argparse
import itertools
import json
import subprocess
import sys
from pathlib import Path


def parse_int(value):
    if isinstance(value, int):
        return value
    return int(str(value), 0)


def cli_int(value):
    return f"-{abs(value):#x}" if value < 0 else f"{value:#x}"


def load_scenario(path):
    with open(path, "r", encoding="utf-8") as stream:
        data = json.load(stream)
    if "probe" not in data or "rom_dir" not in data or "snapshot" not in data:
        raise ValueError("scenario requires probe, rom_dir and snapshot")
    if "dimensions" not in data or not isinstance(data["dimensions"], list):
        raise ValueError("scenario requires a dimensions array")
    return data


def dimension_values(dimension):
    if "values" in dimension:
        return [parse_int(value) for value in dimension["values"]]
    if "bits" in dimension:
        bits = [parse_int(bit) for bit in dimension["bits"]]
        base = parse_int(dimension.get("base", 0))
        values = []
        for mask in range(1 << len(bits)):
            value = base
            for index, bit in enumerate(bits):
                if mask & (1 << index):
                    value |= 1 << bit
            values.append(value)
        return values
    raise ValueError(f"dimension {dimension.get('name', '<unnamed>')} needs values or bits")


def mutation_args(dimension, value):
    kind = dimension.get("kind")
    if kind == "reg":
        return ["--set-reg", f"{dimension['register']}={cli_int(value)}"]
    if kind in {"u8", "u16", "u32"}:
        return [
            f"--set-{kind}",
            f"{parse_int(dimension['address']):#x}={cli_int(value)}",
        ]
    raise ValueError(f"unsupported dimension kind: {kind}")


def run_case(scenario, names, dimensions, values, case_index):
    command = [
        str(scenario["probe"]),
        "--rom-dir",
        str(scenario["rom_dir"]),
        "--snapshot",
        str(scenario["snapshot"]),
        "--max-steps",
        str(parse_int(scenario.get("max_steps", 100000))),
    ]
    if "until" in scenario:
        command += ["--until", f"{parse_int(scenario['until']):#x}"]
    for dimension, value in zip(dimensions, values):
        command += mutation_args(dimension, value)
    for address in scenario.get("read_u32", []):
        command += ["--read-u32", f"{parse_int(address):#x}"]

    completed = subprocess.run(command, text=True, capture_output=True)
    record = {
        "case": case_index,
        "inputs": dict(zip(names, values)),
        "returncode": completed.returncode,
        "stderr": completed.stderr.strip(),
    }
    if completed.stdout.strip():
        lines = [line for line in completed.stdout.splitlines() if line.strip()]
        try:
            parsed = [json.loads(line) for line in lines]
            record["probe"] = parsed
            finals = [item for item in parsed if item.get("type") == "final"]
            if finals:
                record["outcome"] = finals[-1]
        except json.JSONDecodeError:
            record["stdout"] = completed.stdout
    return record


def main():
    parser = argparse.ArgumentParser(
        description="Sweep vf2probe state dimensions from a JSON scenario"
    )
    parser.add_argument("scenario")
    parser.add_argument("--output", required=True)
    parser.add_argument("--limit", type=int, default=0, help="stop after N generated cases")
    args = parser.parse_args()

    scenario = load_scenario(args.scenario)
    dimensions = scenario["dimensions"]
    names = [dimension["name"] for dimension in dimensions]
    value_sets = [dimension_values(dimension) for dimension in dimensions]
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    case_count = 0
    failure_count = 0
    with output_path.open("w", encoding="utf-8") as output:
        for case_index, values in enumerate(itertools.product(*value_sets)):
            if args.limit and case_count >= args.limit:
                break
            record = run_case(scenario, names, dimensions, values, case_index)
            output.write(json.dumps(record, sort_keys=True) + "\n")
            output.flush()
            case_count += 1
            if record["returncode"] != 0:
                failure_count += 1

    print(
        f"sweep complete: {case_count} cases, {failure_count} probe failures",
        file=sys.stderr,
    )
    return 1 if failure_count else 0


if __name__ == "__main__":
    raise SystemExit(main())
