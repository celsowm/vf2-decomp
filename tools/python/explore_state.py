#!/usr/bin/env python3
import argparse
import json
import os
import random
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
    for required in ("probe", "rom_dir", "snapshot", "dimensions"):
        if required not in data:
            raise ValueError(f"scenario requires {required}")
    return data


def dimension_values(dimension):
    if "values" in dimension:
        return [parse_int(value) for value in dimension["values"]]
    bits = [parse_int(bit) for bit in dimension.get("bits", [])]
    if not bits:
        raise ValueError(f"dimension {dimension.get('name', '<unnamed>')} needs values or bits")
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


def build_command(scenario, dimensions, values, output_snapshot):
    command = [
        str(scenario["probe"]),
        "--rom-dir",
        str(scenario["rom_dir"]),
        "--snapshot",
        str(scenario["snapshot"]),
        "--max-steps",
        str(parse_int(scenario.get("max_steps", 100000))),
        "--trace",
        "--output-snapshot",
        str(output_snapshot),
    ]
    if "until" in scenario:
        command += ["--until", f"{parse_int(scenario['until']):#x}"]
    for dimension, value in zip(dimensions, values):
        command += mutation_args(dimension, value)
    for address in scenario.get("read_u32", []):
        command += ["--read-u32", f"{parse_int(address):#x}"]
    return command


def run_candidate(command):
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )
    edges = set()
    final = None
    assert process.stdout is not None
    for line in process.stdout:
        line = line.strip()
        if not line:
            continue
        record = json.loads(line)
        if record.get("type") == "step":
            edges.add((int(record["ip_before"]), int(record["ip_after"])))
        elif record.get("type") == "final":
            final = record
    stderr = process.stderr.read().strip() if process.stderr is not None else ""
    returncode = process.wait()
    return returncode, edges, final, stderr


def mutate(parent, value_sets, rng, max_mutations):
    candidate = list(parent)
    count = rng.randint(1, min(max_mutations, len(candidate)))
    for index in rng.sample(range(len(candidate)), count):
        choices = value_sets[index]
        if len(choices) == 1:
            candidate[index] = choices[0]
            continue
        current = candidate[index]
        alternatives = [value for value in choices if value != current]
        candidate[index] = rng.choice(alternatives)
    return tuple(candidate)


def write_json(path, value):
    with open(path, "w", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write("\n")


def load_existing_corpus(manifest_path, names):
    global_edges = set()
    corpus_inputs = []
    seen_inputs = set()
    if not manifest_path.exists():
        return global_edges, corpus_inputs, seen_inputs

    with manifest_path.open("r", encoding="utf-8") as stream:
        for line in stream:
            if not line.strip():
                continue
            record = json.loads(line)
            inputs = record.get("inputs", {})
            try:
                values = tuple(parse_int(inputs[name]) for name in names)
            except KeyError as error:
                raise ValueError(
                    f"existing corpus does not match current scenario; missing {error.args[0]!r}"
                ) from error
            corpus_inputs.append(values)
            seen_inputs.add(values)
            for edge in record.get("new_edges", []):
                global_edges.add((int(edge["from"]), int(edge["to"])))
    return global_edges, corpus_inputs, seen_inputs


def main():
    parser = argparse.ArgumentParser(
        description="Coverage-guided exploration above vf2probe guest i960 traces"
    )
    parser.add_argument("scenario")
    parser.add_argument("--corpus", required=True)
    parser.add_argument("--iterations", type=int, default=1000)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--max-mutations", type=int, default=3)
    args = parser.parse_args()

    if args.iterations < 1 or args.max_mutations < 1:
        parser.error("iterations and max-mutations must be positive")

    scenario = load_scenario(args.scenario)
    dimensions = scenario["dimensions"]
    names = [dimension["name"] for dimension in dimensions]
    value_sets = [dimension_values(dimension) for dimension in dimensions]
    baseline = tuple(values[0] for values in value_sets)
    rng = random.Random(args.seed)

    corpus_dir = Path(args.corpus)
    corpus_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = corpus_dir / "manifest.jsonl"
    global_edges, corpus_inputs, seen_inputs = load_existing_corpus(manifest_path, names)
    starting_cases = len(corpus_inputs)
    starting_edges = len(global_edges)

    manifest = manifest_path.open("a", encoding="utf-8")
    try:
        for iteration in range(args.iterations):
            if not corpus_inputs:
                candidate = baseline
            else:
                candidate = mutate(
                    rng.choice(corpus_inputs),
                    value_sets,
                    rng,
                    args.max_mutations,
                )
            if candidate in seen_inputs:
                continue
            seen_inputs.add(candidate)

            temp_snapshot = corpus_dir / f".candidate-{os.getpid()}-{iteration}.vf2snap"
            command = build_command(scenario, dimensions, candidate, temp_snapshot)
            returncode, edges, final, stderr = run_candidate(command)
            new_edges = edges - global_edges

            if returncode != 0 or final is None or not temp_snapshot.exists():
                temp_snapshot.unlink(missing_ok=True)
                continue
            if not new_edges:
                temp_snapshot.unlink(missing_ok=True)
                continue

            corpus_index = len(corpus_inputs)
            snapshot_path = corpus_dir / f"case-{corpus_index:05d}.vf2snap"
            record_path = corpus_dir / f"case-{corpus_index:05d}.json"
            temp_snapshot.replace(snapshot_path)
            global_edges.update(edges)
            corpus_inputs.append(candidate)

            record = {
                "case": corpus_index,
                "iteration": iteration,
                "inputs": dict(zip(names, candidate)),
                "new_edge_count": len(new_edges),
                "total_edge_count": len(global_edges),
                "new_edges": [
                    {"from": source, "to": target}
                    for source, target in sorted(new_edges)
                ],
                "final": final,
                "snapshot": str(snapshot_path),
                "stderr": stderr,
            }
            write_json(record_path, record)
            manifest.write(json.dumps(record, sort_keys=True) + "\n")
            manifest.flush()
            print(
                f"accepted case {corpus_index}: +{len(new_edges)} edges "
                f"({len(global_edges)} total)",
                file=sys.stderr,
            )
    finally:
        manifest.close()

    print(
        f"exploration complete: +{len(corpus_inputs) - starting_cases} corpus cases, "
        f"+{len(global_edges) - starting_edges} guest edges "
        f"({len(corpus_inputs)} cases / {len(global_edges)} edges total)",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
