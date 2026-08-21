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


def new_field():
    return {
        "bases": set(),
        "reads": 0,
        "writes": 0,
        "sizes": Counter(),
        "ips": Counter(),
        "addresses": Counter(),
    }


def apply_access(fields, bases, window, access, ip):
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
            if ip is not None:
                field["ips"][ip] += 1
    return matched


def summarize_trace(path, bases, window):
    fields = defaultdict(new_field)
    pending = defaultdict(list)
    total_accesses = 0
    unmatched = 0

    with open(path, "r", encoding="utf-8") as stream:
        for line in stream:
            if not line.strip():
                continue
            record = json.loads(line)
            kind = record.get("type")
            if kind == "memory":
                total_accesses += 1
                pending[int(record["step"])].append(
                    {
                        "kind": record["kind"],
                        "address": int(record["address"]),
                        "size": int(record["size"]),
                    }
                )
            elif kind == "step":
                step = int(record["step"])
                ip = int(record["ip_before"])
                for access in pending.pop(step, []):
                    if not apply_access(fields, bases, window, access, ip):
                        unmatched += 1

    for accesses in pending.values():
        for access in accesses:
            if not apply_access(fields, bases, window, access, None):
                unmatched += 1

    return fields, total_accesses, unmatched


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

    fields, total_accesses, unmatched = summarize_trace(args.trace, bases, args.window)
    records = field_records(fields)
    print_report(records, bases, total_accesses, unmatched, args.min_count, args.limit)

    if args.json_output:
        payload = {
            "trace": args.trace,
            "bases": {name: value for name, value in bases.items()},
            "window": args.window,
            "accesses": total_accesses,
            "unmatched": unmatched,
            "fields": records,
        }
        with open(args.json_output, "w", encoding="utf-8") as stream:
            json.dump(payload, stream, indent=2, sort_keys=True)
            stream.write("\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
