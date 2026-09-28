#!/usr/bin/env python3
"""Validate one exact state-8 field mask through the full native dispatcher.

A calibrated task-entry snapshot at 0x1645c is patched for every measured
distribution/countdown/mode/threshold combination. Each case runs the sequential
ROM executor to the scheduler return at 0x10dcc and the fully native executor
(`native-resume`) over the same patched entry snapshot. Both final snapshots are
compared, including instruction/call/return counters. This is the measurement
harness for the 0x1645c dispatcher admission matrices documented under
decomp/i960/notes.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

from game_info_18644_common import (
    FIGHTER0_PTR_ADDRESS,
    SCHEDULER_RETURN,
    architectural_signature,
    assert_snapshot_ip,
    make_state_fixture,
    read_work_u32,
    run,
    snapshot_counters,
    u32,
    write_work_u32,
    IP_OFFSET,
)

HERE = Path(__file__).resolve().parent

GAME_INFO_TASK_ENTRY = 0x0001645C


def make_full_dispatch_fixture(
    base: bytes,
    state: int,
    flags0: int,
    flags1: int,
    countdown: int,
    mode_bit6: int,
    threshold: int,
) -> bytes:
    """Patch a calibrated 0x1645c entry for one full-dispatch case.

    The measured dispatcher matrices force the fighter-flag bit-31 path on
    both fighter records, mirroring make_bit31_fixture in
    validate_game_info_state4.py.
    """
    data = bytearray(
        make_state_fixture(base, state, flags0, flags1, countdown, mode_bit6, threshold)
    )
    for slot in (FIGHTER0_PTR_ADDRESS, FIGHTER0_PTR_ADDRESS + 4):
        fighter = read_work_u32(base, slot)
        current = read_work_u32(base, fighter)
        write_work_u32(data, fighter, current | 0x80000000)
    return bytes(data)


def build_entry_boundary(binary: Path, roms: Path, directory: Path) -> Path:
    """Reconstruct the calibrated fa_game_info task-entry snapshot."""
    fifth = directory / "fifth.vf2snap"
    entry = directory / "game_info_1645c.vf2snap"
    run([str(binary), "native-fifth-dispatch", str(roms), str(fifth)], quiet=True)
    run(
        [
            str(binary), "resume-trace", str(roms), str(fifth),
            "20000000", "4294967295", "0x80000000", "0", "0",
            str(entry), hex(GAME_INFO_TASK_ENTRY),
        ],
        quiet=True,
    )
    data = entry.read_bytes()
    if u32(data, IP_OFFSET) != GAME_INFO_TASK_ENTRY:
        raise RuntimeError("failed to reconstruct 0x1645c task-entry boundary")
    return entry


def resume_rom(binary: Path, roms: Path, source: Path, output: Path) -> None:
    completed = subprocess.run(
        [
            str(binary), "resume-trace", str(roms), str(source),
            "1000000", "4294967295", "0", "0", "0",
            str(output), hex(SCHEDULER_RETURN),
        ],
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"reference resume failed: {completed.stdout}{completed.stderr}"
        )


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
        raise RuntimeError(
            f"native resume failed: {(completed.stdout + completed.stderr).strip()}"
        )


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
    architecture_equal = (
        architectural_signature(expected_data) == architectural_signature(actual_data)
    )
    equal = completed.returncode == 0 and counter_equal and architecture_equal
    deltas = ", ".join(
        f"{name}={actual_counters[name] - expected_counters[name]:+d}"
        for name in expected_counters
    )
    return equal, (
        f"snapshot={'MATCH' if completed.returncode == 0 else 'DIFF'} "
        f"arch={'eq' if architecture_equal else 'DIFF'} counters[{deltas}]"
    )


def validate_case(
    binary: Path,
    roms: Path,
    base: bytes,
    state: int,
    flags0: int,
    flags1: int,
    countdown: int,
    mode_bit6: int,
    threshold: int,
    directory: Path,
) -> bool:
    stem = (
        f"full-dispatch-s{state}_f{flags0:08x}_{flags1:08x}"
        f"_cd{countdown}_m{mode_bit6}_t{threshold & 0xffffffff:08x}"
    )
    start = directory / f"{stem}_start.vf2snap"
    reference = directory / f"{stem}_reference.vf2snap"
    final = directory / f"{stem}_native.vf2snap"

    start.write_bytes(
        make_full_dispatch_fixture(
            base, state, flags0, flags1, countdown, mode_bit6, threshold
        )
    )
    label = (
        f"f0=0x{flags0:08x} f1=0x{flags1:08x} cd={countdown} "
        f"mode6={mode_bit6} t={threshold:+d}"
    )
    try:
        resume_rom(binary, roms, start, reference)
    except RuntimeError as error:
        print(f"{label} REFERENCE-FAIL {error}")
        return False
    try:
        run_native_task(binary, roms, start, final)
    except RuntimeError as error:
        text = str(error).strip()
        detail = text.splitlines()[-1] if text else "unknown error"
        print(f"{label} NATIVE-FAIL {detail}")
        return False
    equal, detail = compare(binary, reference, final)
    print(f"{label} {'MATCH' if equal else 'DIFF '} {detail}")
    return equal


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("rom_directory", type=Path)
    parser.add_argument(
        "--mask",
        type=lambda value: int(value, 0),
        required=True,
        help="exact combined state-8 field mask admitted by the dispatcher",
    )
    parser.add_argument("--state", type=int, choices=(4, 8), default=8)
    parser.add_argument("--thresholds", default="0,1,2",
                        help="comma-separated nonnegative thresholds")
    parser.add_argument("--base", type=Path,
                        help="reuse a calibrated 0x1645c entry snapshot")
    parser.add_argument("--keep", type=Path, help="keep generated snapshots")
    parser.add_argument(
        "--distributions", default="unilateral,bilateral",
        help="comma-separated distributions: unilateral,bilateral",
    )
    parser.add_argument(
        "--countdowns", default="0,1",
        help="comma-separated countdown values to measure",
    )
    parser.add_argument(
        "--mode-bits", default="0,1",
        help="comma-separated mode-bit-6 values to measure",
    )
    parser.add_argument("--workers", type=int, default=2)
    args = parser.parse_args()

    thresholds = [int(value, 0) for value in args.thresholds.split(",") if value != ""]
    if any(value < 0 for value in thresholds):
        parser.error("--thresholds must be nonnegative for full dispatch")
    distributions = [value.strip() for value in args.distributions.split(",")
                     if value.strip()]
    if not distributions or any(value not in {"unilateral", "bilateral"}
                                for value in distributions):
        parser.error("--distributions must contain unilateral and/or bilateral")
    try:
        countdowns = [int(value, 0) for value in args.countdowns.split(",")
                      if value != ""]
        mode_bits = [int(value, 0) for value in args.mode_bits.split(",")
                     if value != ""]
    except ValueError as error:
        parser.error(f"invalid countdown/mode value: {error}")
    if (not countdowns or any(value not in (0, 1) for value in countdowns) or
            not mode_bits or any(value not in (0, 1) for value in mode_bits)):
        parser.error("--countdowns and --mode-bits values must be 0 or 1")

    if args.keep is not None:
        args.keep.mkdir(parents=True, exist_ok=True)
        root_context = None
        root = args.keep
    else:
        root_context = tempfile.TemporaryDirectory()
        root = Path(root_context.name)
    try:
        if args.base is not None:
            base_snapshot = args.base
        else:
            base_snapshot = build_entry_boundary(args.binary, args.rom_directory, root)
        assert_snapshot_ip(base_snapshot.read_bytes(), GAME_INFO_TASK_ENTRY,
                           "entry boundary")
        base = base_snapshot.read_bytes()

        distributions_flags = []
        if "unilateral" in distributions:
            distributions_flags.extend(((args.mask, 0), (0, args.mask)))
        if "bilateral" in distributions:
            distributions_flags.append((args.mask, args.mask))
        cases = [
            (args.binary, args.rom_directory, base, args.state,
             flags0, flags1, countdown, mode_bit6, threshold, root)
            for flags0, flags1 in distributions_flags
            for countdown in countdowns
            for mode_bit6 in mode_bits
            for threshold in thresholds
        ]
        total = len(cases)
        matched = 0
        failures = 0
        if args.workers <= 1:
            for case in cases:
                matched += int(validate_case(*case))
        else:
            with ThreadPoolExecutor(max_workers=args.workers) as executor:
                futures = [executor.submit(validate_case, *case) for case in cases]
                for future in as_completed(futures):
                    try:
                        matched += int(future.result())
                    except Exception as error:  # noqa: BLE001 - report and continue
                        failures += 1
                        print(f"CASE-ERROR {error}")
        print(f"summary: {matched}/{total} exact")
        raise SystemExit(0 if matched == total and failures == 0 else 1)
    finally:
        if root_context is not None:
            root_context.cleanup()


if __name__ == "__main__":
    main()
