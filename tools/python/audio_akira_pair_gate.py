#!/usr/bin/env python3
"""Regenerate native-vs-original Akira Timer-A-49 PCM from a private ROM.

Executes the existing owner-ROM binary twice as separate processes,
first with the original 68000 sample-page walker and then with its
native C equivalent; original L2, dispatch and the modeled SCSP are
shared. Compares resulting PCM WAVs without retaining either file.

This is an isolated music-preview differential, NOT proof of the full
native song sequencer, cycle-accurate sound CPU scheduling or arcade DAC.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

from audio_pcm_diff import compare_pcm

EXPECTED = (
    ("--akira-tima-original-wav-dir",
     "vf2_akira_10s_ORIGINAL_WALK_TIMA49_scsp_model.wav",
     "AKIRA_MUSIC_ORIGINAL_WALK_PREVIEW"),
    ("--akira-tima-native-wav-dir",
     "vf2_akira_10s_NATIVE_C_WALK_TIMA49_scsp_model.wav",
     "AKIRA_MUSIC_NATIVE_WALK_PREVIEW"),
)


class PairGateError(Exception):
    pass


def run_pair(binary: Path, rom_dir: Path, timeout_seconds: int = 1200) -> dict:
    """Generate both isolated previews and compare temporary WAV outputs."""
    if not binary.is_file() or not rom_dir.is_dir():
        raise PairGateError("binary or private ROM directory is missing")
    if timeout_seconds < 1:
        raise PairGateError("timeout_seconds must be positive")
    with tempfile.TemporaryDirectory(prefix="vf2-akira-parity-") as root:
        outputs = []
        for index, (mode, filename, expected_marker) in enumerate(EXPECTED):
            dest = Path(root) / ("original" if index == 0 else "native")
            dest.mkdir()
            args = [str(binary.resolve()), str(rom_dir.resolve()), mode, str(dest)]
            try:
                proc = subprocess.run(args, capture_output=True, text=True,
                                      timeout=timeout_seconds, check=False)
            except (OSError, subprocess.TimeoutExpired) as exc:
                raise PairGateError(f"{mode}: could not run: {exc}") from exc
            if proc.returncode != 0:
                raise PairGateError(f"{mode}: original-ROM binary exited {proc.returncode}")
            if (expected_marker not in proc.stdout or
                "AKIRA_MUSIC_SEQUENCE_FINISH" not in proc.stdout):
                raise PairGateError(f"{mode}: missing expected execution markers")
            output = dest / filename
            if not output.is_file():
                raise PairGateError(f"{mode}: missing generated WAV")
            outputs.append(output)
        try:
            report = compare_pcm(outputs[0], outputs[1])
        except (OSError, EOFError, ValueError, wave.Error) as exc:
            raise PairGateError(f"generated WAV could not be compared: {exc}") from exc
        if report["sample_rate"] != 44100 or report["frames"] != 441000 or \
                report["channels"] != 2 or report["sample_width_bytes"] != 2:
            raise PairGateError("generated WAV has unexpected Timer-A-49 preview format")
        report["run_modes"] = [entry[0] for entry in EXPECTED]
        report["scope"] = "original_l2_dispatch_scsp_model_native_vs_original_pcm_walker"
        report["rom_directory_supplied_by_caller"] = True
        report["arcade_timing_or_dac_fidelity_proven"] = False
        return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path, help="built vf2_m68k_l3_selection_tests")
    parser.add_argument("rom_dir", type=Path, help="private owner ROM directory")
    parser.add_argument("--timeout-seconds", type=int, default=1200)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    try:
        result = run_pair(args.binary, args.rom_dir, args.timeout_seconds)
    except PairGateError as exc:
        print(f"akira-pair-gate: refused: {exc}", file=sys.stderr)
        return 2
    if args.json:
        print(json.dumps(result, sort_keys=True))
    else:
        print("akira-pair-gate: " + result["status"])
        print("frames={} first_different_frame={}".format(
            result["frames"], result["first_different_frame"]))
        print("timing/DAC fidelity proven: false")
    return 0 if result["status"] == "match" else 1


if __name__ == "__main__":
    raise SystemExit(main())
