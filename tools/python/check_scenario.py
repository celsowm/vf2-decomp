#!/usr/bin/env python3
import argparse
import json


def parse_int(value):
    if isinstance(value, int):
        return value
    return int(str(value), 0)


def main():
    parser = argparse.ArgumentParser(
        description="Validate a vf2 sweep scenario without executing it"
    )
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
            address = parse_int(dimension["address"])
            if address < 0 or address > 0xFFFFFFFF:
                raise SystemExit(f"dimension {name} has invalid address")
        if ("values" in dimension) == ("bits" in dimension):
            raise SystemExit(f"dimension {name} requires exactly one of values or bits")
        if "values" in dimension:
            values = [parse_int(value) for value in dimension["values"]]
            if not values:
                raise SystemExit(f"dimension {name} has no values")
            if any(value < -0x80000000 or value > 0xFFFFFFFF for value in values):
                raise SystemExit(f"dimension {name} contains a value outside 32 bits")
            total *= len(values)
        else:
            bits = [parse_int(bit) for bit in dimension["bits"]]
            base = parse_int(dimension.get("base", 0))
            if len(set(bits)) != len(bits) or any(bit < 0 or bit > 31 for bit in bits):
                raise SystemExit(f"dimension {name} has invalid/repeated bit positions")
            if base < 0 or base > 0xFFFFFFFF:
                raise SystemExit(f"dimension {name} has an invalid base mask")
            if any(base & (1 << bit) for bit in bits):
                raise SystemExit(f"dimension {name} base overlaps swept bits")
            total *= 1 << len(bits)

    if "until" in data:
        until = parse_int(data["until"])
        if until < 0 or until > 0xFFFFFFFF:
            raise SystemExit("until is outside the 32-bit address space")
    for address_value in data.get("read_u32", []):
        address = parse_int(address_value)
        if address < 0 or address > 0xFFFFFFFF:
            raise SystemExit("read_u32 contains an invalid address")

    print(f"valid scenario: {len(data['dimensions'])} dimensions, {total} generated cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
