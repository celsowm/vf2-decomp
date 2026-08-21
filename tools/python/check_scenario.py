#!/usr/bin/env python3
import argparse
import json


def parse_int(value):
    if isinstance(value, int):
        return value
    return int(str(value), 0)


def main():
    parser = argparse.ArgumentParser(description="Validate a vf2 sweep scenario without executing it")
    parser.add_argument("scenario")
    args = parser.parse_args()

    with open(args.scenario, "r", encoding="utf-8") as stream:
        data = json.load(stream)

    for required in ("probe", "rom_dir", "snapshot", "dimensions"):
        if required not in data:
            raise SystemExit(f"missing required field: {required}")

    seen = set()
    total = 1
    for dimension in data["dimensions"]:
        name = dimension.get("name")
        kind = dimension.get("kind")
        if not name or name in seen:
            raise SystemExit(f"invalid or duplicate dimension name: {name!r}")
        seen.add(name)
        if kind not in {"reg", "u8", "u16", "u32"}:
            raise SystemExit(f"unsupported dimension kind for {name}: {kind!r}")
        if kind == "reg" and not dimension.get("register"):
            raise SystemExit(f"register dimension {name} requires register")
        if kind != "reg":
            parse_int(dimension["address"])
        if ("values" in dimension) == ("bits" in dimension):
            raise SystemExit(f"dimension {name} requires exactly one of values or bits")
        if "values" in dimension:
            values = [parse_int(value) for value in dimension["values"]]
            if not values:
                raise SystemExit(f"dimension {name} has no values")
            total *= len(values)
        else:
            bits = [parse_int(bit) for bit in dimension["bits"]]
            if len(set(bits)) != len(bits) or any(bit < 0 or bit > 31 for bit in bits):
                raise SystemExit(f"dimension {name} has invalid/repeated bit positions")
            total *= 1 << len(bits)

    if "until" in data:
        parse_int(data["until"])
    for address in data.get("read_u32", []):
        parse_int(address)

    print(f"valid scenario: {len(data['dimensions'])} dimensions, {total} generated cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
