#!/usr/bin/env python3
import argparse
import json
from collections import Counter, defaultdict


def parse_int(value):
    if isinstance(value, int):
        return value
    return int(str(value), 0)


def parse_base(text):
    if "=" not in text:
        raise argparse.ArgumentTypeError("base must be name=address")
    name, value = text.split("=", 1)
    if not name:
        raise argparse.ArgumentTypeError("base name must not be empty")
    return name, parse_int(value)


def load_scenario_bases(path):
    if path is None:
        return {}
    with open(path, "r", encoding="utf-8") as stream:
        scenario = json.load(stream)
    metadata = scenario.get("metadata", {})
    bases = {}
    for name in ("fighter0", "fighter1"):
        if name in metadata:
            bases[name] = parse_int(metadata[name])
    return bases


def load_trace(path):
    steps = {}
    accesses = []
    with open(path, "r", encoding="utf-8") as stream:
        for line_number, line in enumerate(stream, 1):
            if not line.strip():
                continue
            record = json.loads(line)
            kind = record.get("type")
            if kind == "step":
                steps[int(record["step"])] = int(record["ip_before"])
            elif kind == "memory":
                accesses.append(
                    {
                        "line": line_number,
                        "step": int(record["step"]),
                        "kind": record["kind"],
                        "address": int(record["address"]),
                        "size": int(record["size"]),
                        "bytes": record.get("bytes", ""),
                    }
                )
    for access in accesses:
        access["ip"] = steps.get(access["step"])
    return steps, accesses


def summarize(bases, accesses, window):
    fields = defaultdict(lambda: {
        "bases": set(),
        "reads": 0,
        "writes": 0,
        "sizes": Counter(),
        "ips": Counter(),
        "addresses": Counter(),
    })
    unmatched = 0
    for access in accesses:
        matched = False
        for base_name, base in bases.items():
            offset = access["address"] - base
            if 0 <= offset < window:
                matched = True
                field = fields[offset]
                field["bases"].add(base_name)
                field["sizes"][access["size"]] += 1
                field["addresses"][access["address"]] += 1
                if access["kind"] == "read":
                    field["reads"] += 1
                else:
                    field["writes"] += 1
                if access["ip"] is not None:
                    field["ips"][access["ip"]] += 1
        if not matched:
            unmatched += 1
    return fields, unmatched


def field_records(fields):
    records = []
    for offset, data in fields.items():
        total = data["reads"] + data["writes"]
        records.append(
            {
                "offset": offset,
                "bases": sorted(data["bases"]),
                "base_count": len(data["bases"]),
                "reads": data["reads"],
                "writes": data["writes"],
                "total": total,
                "sizes": [
                    {"size": size, "count": count}
                    for size, count in sorted(data["sizes"].items())
                ],
                "ips": [
                    {"ip": ip, "count": count}
                    for ip, count in data["ips"].most_common()
                ],
                "addresses": [
                    {"address": address, "count": count}
                    for address, count in data["addresses"].most_common()
                ],
            }
        )
    records.sort(key=lambda item: (-item["base_count"], -item["total"], item["offset"]))
    return records


def print_report(records, bases, total_accesses, unmatched, min_count, limit):
    print("candidate object fields")
    print(f"trace accesses: {total_accesses}")
    print("bases: " + ", ".join(f"{name}=0x{value:08x}" for name, value in bases.items()))
    print(f"unmatched accesses: {unmatched}")
    print()
    shown = 0
    for record in records:
        if record["total"] < min_count:
            continue
        if limit and shown >= limit:
            break
        size_text = ",".join(
            f"{item['size']}B×{item['count']}" for item in record["sizes"]
        )
        ip_text = ", ".join(
            f"0x{item['ip']:08x}×{item['count']}" for item in record["ips"][:6]
        )
        print(
            f"+0x{record['offset']:04x}  bases={','.join(record['bases'])}  "
            f"R={record['reads']} W={record['writes']}  sizes={size_text}"
        )
        if ip_text:
            print(f"  ips: {ip_text}")
        shown += 1


def main():
    parser = argparse.ArgumentParser(
        description="Infer repeated base+offset object fields from vf2probe memory traces"
    )
    parser.add_argument("trace")
    parser.add_argument("--scenario", help="load fighter0/fighter1 bases from scenario metadata")
    parser.add_argument(
        "--base",
        action="append",
        default=[],
        type=parse_base,
        metavar="NAME=ADDRESS",
        help="additional/override object base; repeatable",
    )
    parser.add_argument("--window", type=lambda value: int(value, 0), default=0x2000)
    parser.add_argument("--min-count", type=int, default=1)
    parser.add_argument("--limit", type=int, default=100)
    parser.add_argument("--json", dest="json_output")
    args = parser.parse_args()

    if args.window < 1 or args.min_count < 1 or args.limit < 0:
        parser.error("window/min-count must be positive and limit must be non-negative")

    bases = load_scenario_bases(args.scenario)
    bases.update(dict(args.base))
    if not bases:
        parser.error("provide --scenario with fighter metadata or at least one --base")

    _, accesses = load_trace(args.trace)
    fields, unmatched = summarize(bases, accesses, args.window)
    records = field_records(fields)
    print_report(records, bases, len(accesses), unmatched, args.min_count, args.limit)

    if args.json_output:
        payload = {
            "trace": args.trace,
            "bases": {name: value for name, value in bases.items()},
            "window": args.window,
            "accesses": len(accesses),
            "unmatched": unmatched,
            "fields": records,
        }
        with open(args.json_output, "w", encoding="utf-8") as stream:
            json.dump(payload, stream, indent=2, sort_keys=True)
            stream.write("\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
