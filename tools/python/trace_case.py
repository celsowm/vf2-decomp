#!/usr/bin/env python3
import argparse
import json
import subprocess
import sys

from sweep_state import dimension_values, mutation_args, parse_int


def parse_override(text):
    if "=" not in text:
        raise argparse.ArgumentTypeError("override must be name=value")
    name, value = text.split("=", 1)
    if not name:
        raise argparse.ArgumentTypeError("override name must not be empty")
    return name, parse_int(value)


def load_scenario(path):
    with open(path, "r", encoding="utf-8") as stream:
        data = json.load(stream)
    for required in ("probe", "rom_dir", "snapshot", "dimensions"):
        if required not in data:
            raise ValueError(f"scenario requires {required}")
    return data


def main():
    parser = argparse.ArgumentParser(
        description="Run one measured vf2probe scenario case with memory tracing"
    )
    parser.add_argument("scenario")
    parser.add_argument("--output", required=True)
    parser.add_argument(
        "--set",
        action="append",
        default=[],
        type=parse_override,
        metavar="NAME=VALUE",
        help="override one scenario dimension; repeatable",
    )
    args = parser.parse_args()

    scenario = load_scenario(args.scenario)
    dimensions = scenario["dimensions"]
    by_name = {dimension["name"]: dimension for dimension in dimensions}
    overrides = dict(args.set)
    unknown = sorted(set(overrides) - set(by_name))
    if unknown:
        raise SystemExit(f"unknown dimensions: {', '.join(unknown)}")

    values = {}
    command = [
        str(scenario["probe"]),
        "--rom-dir",
        str(scenario["rom_dir"]),
        "--snapshot",
        str(scenario["snapshot"]),
        "--max-steps",
        str(parse_int(scenario.get("max_steps", 100000))),
        "--memory-trace",
    ]
    if "until" in scenario:
        command += ["--until", f"{parse_int(scenario['until']):#x}"]

    for dimension in dimensions:
        name = dimension["name"]
        value = overrides.get(name)
        if value is None:
            candidates = dimension_values(dimension)
            if not candidates:
                raise SystemExit(f"dimension {name} has no candidate values")
            value = candidates[0]
        values[name] = value
        command += mutation_args(dimension, value)

    with open(args.output, "w", encoding="utf-8") as output:
        completed = subprocess.run(
            command,
            text=True,
            stdout=output,
            stderr=subprocess.PIPE,
        )

    print(
        f"trace case wrote {args.output}: returncode={completed.returncode} inputs={values}",
        file=sys.stderr,
    )
    if completed.stderr.strip():
        print(completed.stderr.rstrip(), file=sys.stderr)
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())
