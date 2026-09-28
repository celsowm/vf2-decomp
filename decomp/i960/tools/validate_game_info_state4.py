#!/usr/bin/env python3
"""Validate state-4/state-8 flag combinations through the full game-info chain."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from game_info_18644_common import (
    GAME_INFO_FIRST_RETURN,
    GAME_INFO_SECOND_CALL,
    GAME_INFO_SECOND_RETURN,
    SCHEDULER_RETURN,
    architectural_signature,
    build_boundary,
    make_state_fixture,
    read_work_u32,
    resume_rom,
    snapshot_counters,
    write_work_u32,
)

HERE = Path(__file__).resolve().parent
CHILD_RUNNER = HERE / "run_game_info_child.py"

STATE_FLAGS = {
    4: (1 << 6, 1 << 14, 1 << 15, 1 << 16),
    8: (1 << 1, 1 << 2, 1 << 4),
}


def run_child(binary: Path, roms: Path, source: Path, output: Path, return_address: int) -> None:
    if os.name == "nt":
        completed = subprocess.run(
            [
                str(binary), "game-info-child", str(roms), str(source),
                str(output), hex(return_address),
            ],
            capture_output=True,
            text=True,
        )
        if completed.returncode != 0:
            raise RuntimeError(completed.stderr or completed.stdout)
        return
    completed = subprocess.run(
        [
            sys.executable,
            str(CHILD_RUNNER),
            str(binary),
            str(roms),
            str(source),
            str(output),
            "--return-address",
            hex(return_address),
        ],
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr or completed.stdout)


def run_native_task(binary: Path, roms: Path, source: Path, output: Path) -> None:
    completed = subprocess.run(
        [
            str(binary), "native-resume", str(roms), str(source),
            "1000", "0", hex(SCHEDULER_RETURN), str(output),
        ],
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr or completed.stdout)


def make_bit31_fixture(
    base: bytes,
    state: int,
    flags0: int,
    flags1: int,
    countdown: int,
    mode_bit6: int,
    threshold: int,
) -> bytes:
    data = bytearray(
        make_state_fixture(
            base, state, flags0, flags1, countdown, mode_bit6, threshold
        )
    )
    fighter0 = read_work_u32(base, 0x00500804)
    fighter1 = read_work_u32(base, 0x00500808)
    for fighter in (fighter0, fighter1):
        current = read_work_u32(base, fighter)
        write_work_u32(data, fighter, current | 0x80000000)
    return bytes(data)


def compare(binary: Path, expected: Path, actual: Path) -> tuple[bool, str]:
    completed = subprocess.run(
        [str(binary), "compare-snapshots", str(expected), str(actual)],
        capture_output=True,
        text=True,
    )
    expected_data = expected.read_bytes()
    actual_data = actual.read_bytes()
    expected_counters = snapshot_counters(expected_data)
    actual_counters = snapshot_counters(actual_data)
    counter_equal = expected_counters == actual_counters
    architecture_equal = architectural_signature(expected_data) == architectural_signature(actual_data)
    equal = completed.returncode == 0 and counter_equal and architecture_equal
    deltas = ", ".join(
        f"{name}={actual_counters[name] - expected_counters[name]:+d}"
        for name in expected_counters
    )
    return equal, f"snapshot={'MATCH' if completed.returncode == 0 else 'DIFF'} counters[{deltas}]"


def validate_case(
    binary: Path,
    roms: Path,
    base: bytes,
    flags0: int,
    flags1: int,
    countdown: int,
    mode_bit6: int,
    state: int,
    threshold: int,
    directory: Path,
    fifth_base: bytes | None = None,
) -> bool:
    stem = f"s{state}_f{flags0:05x}_{flags1:05x}_cd{countdown}_m{mode_bit6}_t{threshold & 0xffffffff:08x}"
    start = directory / f"{stem}_start.vf2snap"
    reference = directory / f"{stem}_reference.vf2snap"
    child1 = directory / f"{stem}_child1.vf2snap"
    second_call = directory / f"{stem}_second_call.vf2snap"
    child2 = directory / f"{stem}_child2.vf2snap"
    final = directory / f"{stem}_final.vf2snap"

    if threshold < 0:
        if fifth_base is None:
            raise RuntimeError("negative-threshold validation requires the 0x1645c base")
        start.write_bytes(
            make_bit31_fixture(
                fifth_base, state, flags0, flags1, countdown, mode_bit6, threshold
            )
        )
        resume_rom(binary, roms, start, reference, SCHEDULER_RETURN)
        run_native_task(binary, roms, start, final)
        equal, detail = compare(binary, reference, final)
        print(
            f"f0=0x{flags0:05x} f1=0x{flags1:05x} cd={countdown} "
            f"mode6={mode_bit6} threshold={threshold:+d} "
            f"{'MATCH' if equal else 'DIFF '} {detail}"
        )
        return equal

    start.write_bytes(
        make_state_fixture(
            base, state, flags0, flags1, countdown, mode_bit6, threshold
        )
    )
    resume_rom(binary, roms, start, reference, SCHEDULER_RETURN)
    run_child(binary, roms, start, child1, GAME_INFO_FIRST_RETURN)
    resume_rom(binary, roms, child1, second_call, GAME_INFO_SECOND_CALL)
    run_child(binary, roms, second_call, child2, GAME_INFO_SECOND_RETURN)
    resume_rom(binary, roms, child2, final, SCHEDULER_RETURN)
    equal, detail = compare(binary, reference, final)
    print(
        f"f0=0x{flags0:05x} f1=0x{flags1:05x} cd={countdown} "
        f"mode6={mode_bit6} threshold={threshold:+d} "
        f"{'MATCH' if equal else 'DIFF '} {detail}"
    )
    return equal


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("rom_directory", type=Path)
    parser.add_argument("--mask", type=lambda value: int(value, 0), help="one state flag mask")
    parser.add_argument("--all", action="store_true", help="run all masks for the selected state")
    parser.add_argument("--state", type=int, choices=(4, 8), default=4)
    parser.add_argument(
        "--include-bit8",
        action="store_true",
        help="for state 8, include flag bit 8 in the mask matrix",
    )
    parser.add_argument(
        "--extra-bit",
        action="append",
        type=lambda value: int(value, 0),
        default=[],
        help="append an arbitrary fighter flag bit (0..31) to the mask matrix",
    )
    parser.add_argument("--threshold", type=lambda value: int(value, 0), default=0)
    parser.add_argument("--base", type=Path, help="reuse a calibrated 0x164ac snapshot")
    parser.add_argument("--keep", type=Path, help="keep generated snapshots")
    args = parser.parse_args()
    if not args.all and args.mask is None:
        parser.error("provide --mask or --all")
    bit_flags = STATE_FLAGS[args.state]
    if args.include_bit8:
        if args.state != 8:
            parser.error("--include-bit8 is only valid with --state 8")
        bit_flags = bit_flags + (1 << 8,)
    if any(bit < 0 or bit > 31 for bit in args.extra_bit):
        parser.error("--extra-bit values must be in 0..31")
    extra_flags = tuple(1 << bit for bit in args.extra_bit)
    if len(set(bit_flags + extra_flags)) != len(bit_flags) + len(extra_flags):
        parser.error("--extra-bit values must not duplicate an existing matrix bit")
    bit_flags = bit_flags + extra_flags
    mask_count = 1 << len(bit_flags)
    if args.mask is not None and not 0 <= args.mask < mask_count:
        parser.error(f"--mask must be in 0..{mask_count - 1} for state {args.state}")

    if args.keep is not None:
        args.keep.mkdir(parents=True, exist_ok=True)
        root_context = None
        root = args.keep
    else:
        root_context = tempfile.TemporaryDirectory()
        root = Path(root_context.name)
    try:
        if args.base is not None:
            base = args.base.read_bytes()
            fifth_base = None
        else:
            boundary = build_boundary(args.binary, args.rom_directory, root)
            base = boundary.read_bytes()
            fifth_base = (root / "fifth.vf2snap").read_bytes()
        masks = range(mask_count) if args.all else (args.mask,)
        total = matched = 0
        for mask in masks:
            flags = sum(bit for index, bit in enumerate(bit_flags) if mask & (1 << index))
            for flags0, flags1 in ((flags, 0), (0, flags), (flags, flags)):
                for countdown in (0, 1):
                    for mode_bit6 in (0, 1):
                        total += 1
                        matched += int(
                            validate_case(
                                args.binary,
                                args.rom_directory,
                                base,
                                flags0,
                                flags1,
                                countdown,
                                mode_bit6,
                                args.state,
                                args.threshold,
                                root,
                                fifth_base,
                            )
                        )
        print(f"summary: {matched}/{total} exact")
        raise SystemExit(0 if matched == total else 1)
    finally:
        if root_context is not None:
            root_context.cleanup()


if __name__ == "__main__":
    main()
