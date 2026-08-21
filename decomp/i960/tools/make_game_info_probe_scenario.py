#!/usr/bin/env python3
"""Build a measured vf2probe scenario for fa_game_info 0x18644 recovery."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path

from game_info_18644_common import (
    COUNTDOWN_ADDRESS,
    FIGHTER0_PTR_ADDRESS,
    FIGHTER1_PTR_ADDRESS,
    MODE_BASE_PTR_ADDRESS,
    MODE_BIT6,
    MODE_OFFSET,
    SCHEDULER_RETURN,
    STATE4_BYTE_OFFSET,
    STATE_OFFSET,
    build_boundary,
    read_work_u32,
    work_offset,
)

THRESHOLD_ADDRESS = 0x0050A028
DEFAULT_STATE4_BITS = (6, 14, 15, 16)
DEFAULT_STATE8_BITS = (1, 2, 4, 6, 8)


def int_auto(text: str) -> int:
    return int(text, 0)


def parse_bits(text: str) -> list[int]:
    values = [int_auto(item.strip()) for item in text.split(",") if item.strip()]
    if not values or len(set(values)) != len(values):
        raise argparse.ArgumentTypeError("bits must be a non-empty unique comma-separated list")
    if any(bit < 0 or bit > 31 for bit in values):
        raise argparse.ArgumentTypeError("bit positions must be in 0..31")
    return values


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate a snapshot-resolved vf2probe scenario for game-info 0x18644"
    )
    parser.add_argument("vf2i960", type=Path)
    parser.add_argument("vf2probe", type=Path)
    parser.add_argument("rom_directory", type=Path)
    parser.add_argument("output_scenario", type=Path)
    parser.add_argument("--state", type=int, choices=(4, 8), default=8)
    parser.add_argument(
        "--bits",
        type=parse_bits,
        help="comma-separated flag bits; defaults to the measured state family",
    )
    parser.add_argument(
        "--threshold",
        action="append",
        type=int_auto,
        help="repeatable signed/unsigned threshold; default is 0",
    )
    parser.add_argument("--max-steps", type=int_auto, default=1_000_000)
    return parser.parse_args()


def hex32(value: int) -> str:
    return f"0x{value & 0xFFFFFFFF:08x}"


def main() -> None:
    args = parse_args()
    bits = args.bits
    if bits is None:
        bits = list(DEFAULT_STATE4_BITS if args.state == 4 else DEFAULT_STATE8_BITS)
    thresholds = args.threshold if args.threshold is not None else [0]
    if args.max_steps < 1:
        raise SystemExit("--max-steps must be positive")
    if any(value < -0x80000000 or value > 0xFFFFFFFF for value in thresholds):
        raise SystemExit("threshold values must fit the 32-bit probe mutation domain")

    args.output_scenario.parent.mkdir(parents=True, exist_ok=True)
    snapshot_path = args.output_scenario.with_suffix(".boundary.vf2snap")

    with tempfile.TemporaryDirectory() as temporary:
        boundary_path = build_boundary(
            args.vf2i960,
            args.rom_directory,
            Path(temporary),
        )
        boundary = boundary_path.read_bytes()
    snapshot_path.write_bytes(boundary)

    fighter0 = read_work_u32(boundary, FIGHTER0_PTR_ADDRESS)
    fighter1 = read_work_u32(boundary, FIGHTER1_PTR_ADDRESS)
    mode_address = read_work_u32(boundary, MODE_BASE_PTR_ADDRESS) + MODE_OFFSET
    mode_value = boundary[work_offset(boundary, mode_address)]
    mode_base = mode_value & ~MODE_BIT6

    fighter0_flags = fighter0 + STATE_OFFSET
    fighter1_flags = fighter1 + STATE_OFFSET
    fighter0_selector = fighter0 + STATE4_BYTE_OFFSET
    fighter1_selector = fighter1 + STATE4_BYTE_OFFSET

    scenario = {
        "probe": str(args.vf2probe),
        "rom_dir": str(args.rom_directory),
        "snapshot": str(snapshot_path),
        "until": hex(SCHEDULER_RETURN),
        "max_steps": args.max_steps,
        "metadata": {
            "source": "fa_game_info 0x18644 measured boundary",
            "state": args.state,
            "fighter0": hex32(fighter0),
            "fighter1": hex32(fighter1),
            "fighter0_flags": hex32(fighter0_flags),
            "fighter1_flags": hex32(fighter1_flags),
            "fighter0_selector": hex32(fighter0_selector),
            "fighter1_selector": hex32(fighter1_selector),
            "countdown": hex32(COUNTDOWN_ADDRESS),
            "mode": hex32(mode_address),
            "threshold": hex32(THRESHOLD_ADDRESS),
        },
        "dimensions": [
            {
                "name": "fighter0_selector",
                "kind": "u8",
                "address": hex32(fighter0_selector),
                "values": [args.state],
            },
            {
                "name": "fighter1_selector",
                "kind": "u8",
                "address": hex32(fighter1_selector),
                "values": [args.state],
            },
            {
                "name": "fighter0_flags",
                "kind": "u32",
                "address": hex32(fighter0_flags),
                "base": "0x0",
                "bits": bits,
            },
            {
                "name": "fighter1_flags",
                "kind": "u32",
                "address": hex32(fighter1_flags),
                "base": "0x0",
                "bits": bits,
            },
            {
                "name": "countdown",
                "kind": "u8",
                "address": hex32(COUNTDOWN_ADDRESS),
                "values": [0, 1],
            },
            {
                "name": "mode",
                "kind": "u8",
                "address": hex32(mode_address),
                "base": hex(mode_base),
                "bits": [6],
            },
            {
                "name": "threshold",
                "kind": "u32",
                "address": hex32(THRESHOLD_ADDRESS),
                "values": thresholds,
            },
        ],
        "read_u32": [
            hex32(fighter0_flags),
            hex32(fighter1_flags),
            hex32(THRESHOLD_ADDRESS),
        ],
    }

    args.output_scenario.write_text(
        json.dumps(scenario, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    mask_cases = 1 << len(bits)
    total_cases = mask_cases * mask_cases * 2 * 2 * len(thresholds)
    print(f"wrote measured scenario: {args.output_scenario}")
    print(f"boundary snapshot: {snapshot_path}")
    print(
        f"fighters={hex32(fighter0)}/{hex32(fighter1)} "
        f"mode={hex32(mode_address)} state={args.state} bits={bits}"
    )
    print(f"full Cartesian sweep size: {total_cases} cases")


if __name__ == "__main__":
    main()
