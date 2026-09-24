#!/usr/bin/env python3
"""Validate an ordered 0x18644 state pair with the full native/native chain."""

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
    make_fixture,
    resume_rom,
    snapshot_counters,
)

HERE = Path(__file__).resolve().parent
CHILD_RUNNER = HERE / "run_game_info_child.py"


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


def compare(binary: Path, expected: Path, actual: Path) -> tuple[bool, str]:
    completed = subprocess.run(
        [str(binary), "compare-snapshots", str(expected), str(actual)],
        capture_output=True,
        text=True,
    )
    expected_data = expected.read_bytes()
    actual_data = actual.read_bytes()
    counters_expected = snapshot_counters(expected_data)
    counters_actual = snapshot_counters(actual_data)
    counter_equal = counters_expected == counters_actual
    architecture_equal = architectural_signature(expected_data) == architectural_signature(actual_data)
    equal = completed.returncode == 0 and counter_equal and architecture_equal
    deltas = ", ".join(
        f"{name}={counters_actual[name] - counters_expected[name]:+d}"
        for name in counters_expected
    )
    detail = f"snapshot={'MATCH' if completed.returncode == 0 else 'DIFF'} counters[{deltas}]"
    return equal, detail


def validate_case(
    binary: Path,
    roms: Path,
    base: bytes,
    state0: int,
    state1: int,
    countdown: int,
    mode_bit6: int,
    directory: Path,
) -> bool:
    stem = f"s{state0:03x}_{state1:03x}_cd{countdown}_m{mode_bit6}"
    start = directory / f"{stem}_start.vf2snap"
    reference = directory / f"{stem}_reference.vf2snap"
    child1 = directory / f"{stem}_child1.vf2snap"
    second_call = directory / f"{stem}_second_call.vf2snap"
    child2 = directory / f"{stem}_child2.vf2snap"
    final = directory / f"{stem}_final.vf2snap"

    start.write_bytes(make_fixture(base, state0, state1, countdown, mode_bit6))
    resume_rom(binary, roms, start, reference, SCHEDULER_RETURN)
    run_child(binary, roms, start, child1, GAME_INFO_FIRST_RETURN)
    resume_rom(binary, roms, child1, second_call, GAME_INFO_SECOND_CALL)
    run_child(binary, roms, second_call, child2, GAME_INFO_SECOND_RETURN)
    resume_rom(binary, roms, child2, final, SCHEDULER_RETURN)
    equal, detail = compare(binary, reference, final)
    print(
        f"0x{state0:03x}->0x{state1:03x} cd={countdown} mode6={mode_bit6} "
        f"{'MATCH' if equal else 'DIFF '} {detail}"
    )
    return equal


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("rom_directory", type=Path)
    parser.add_argument("state0", type=lambda value: int(value, 0))
    parser.add_argument("state1", type=lambda value: int(value, 0))
    parser.add_argument("--reverse", action="store_true", help="also validate state1/state0")
    parser.add_argument("--base", type=Path, help="reuse a calibrated 0x164ac snapshot")
    parser.add_argument("--keep", type=Path, help="keep generated snapshots in this directory")
    args = parser.parse_args()

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
        else:
            base = build_boundary(args.binary, args.rom_directory, root).read_bytes()
        pairs = [(args.state0, args.state1)]
        if args.reverse and args.state0 != args.state1:
            pairs.append((args.state1, args.state0))
        total = 0
        matched = 0
        for state0, state1 in pairs:
            for countdown in (0, 1):
                for mode_bit6 in (0, 1):
                    total += 1
                    matched += int(
                        validate_case(
                            args.binary,
                            args.rom_directory,
                            base,
                            state0,
                            state1,
                            countdown,
                            mode_bit6,
                            root,
                        )
                    )
        print(f"summary: {matched}/{total} exact")
        raise SystemExit(0 if matched == total else 1)
    finally:
        if root_context is not None:
            root_context.cleanup()


if __name__ == "__main__":
    main()
