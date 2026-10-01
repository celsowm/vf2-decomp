#!/usr/bin/env python3
"""Sweep the positive state-8 bit-6 composition space and report exactness.

This wraps ``validate_game_info_state4.py`` to enumerate every flag mask that
has bit 6 set inside the measured positive composition space and to emit one
JSONL record per mask. The mask space is the cross product of the measured
low bits (1, 2, 4), bit 8, bit 6 and the measured high bits (21, 26, 29, 30,
31); masks without bit 6 belong to other gates and are skipped.

The tool never decides behavior: it only runs the ROM-backed differential
validator and aggregates the result. It exists so the compact positive bit-6
rule can be proved exhaustively with a resumable, parallelizable driver.

Example:

    python3 decomp/i960/tools/validate_game_info_positive_bit6_sweep.py \
      ./build/vf2i960 /path/to/vf2-roms out/posbit6.jsonl \
      --base out/state8-positive.boundary.vf2snap \
      --shard 0 --shards 4
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent

# bit_flags order used for the validator invocation below.
# index: 0=b1 1=b2 2=b4 3=b8 4=b6 5=21 6=26 7=29 8=30 9=31
BIT6_INDEX = 4
MASK_COUNT = 1 << 10


def parse_masks(text: str) -> list[int]:
    masks = []
    for part in text.split(","):
        part = part.strip()
        if part:
            masks.append(int(part, 0))
    return masks


def bit6_masks(shard: int, shards: int, explicit=None):
    if explicit is not None:
        for index, mask in enumerate(explicit):
            if index % shards == shard:
                yield mask
        return
    for mask in range(MASK_COUNT):
        if not (mask & (1 << BIT6_INDEX)):
            continue
        if mask % shards != shard:
            continue
        yield mask


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("rom_directory", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--threshold", type=lambda v: int(v, 0), default=0)
    parser.add_argument("--shard", type=int, default=0)
    parser.add_argument("--shards", type=int, default=1)
    parser.add_argument(
        "--masks",
        type=parse_masks,
        default=None,
        help="explicit comma-separated mask list instead of the full sweep",
    )
    parser.add_argument("--keep", type=Path)
    args = parser.parse_args()
    if not 0 <= args.shard < args.shards:
        parser.error("shard must satisfy 0 <= shard < shards")

    validator = HERE / "validate_game_info_state4.py"
    command = [
        sys.executable,
        str(validator),
        str(args.binary),
        str(args.rom_directory),
        "--state", "8",
        "--include-bit8",
        "--extra-bit", "6",
        "--extra-bit", "21",
        "--extra-bit", "26",
        "--extra-bit", "29",
        "--extra-bit", "30",
        "--extra-bit", "31",
        "--threshold", str(args.threshold),
        "--base", str(args.base),
    ]
    if args.keep is not None:
        command += ["--keep", str(args.keep)]

    resolved = 0
    exact = 0
    with args.output.open("w", encoding="utf-8") as stream:
        for mask in bit6_masks(args.shard, args.shards, args.masks):
            completed = subprocess.run(
                command + ["--mask", str(mask)],
                capture_output=True,
                text=True,
            )
            summary = ""
            for line in completed.stdout.splitlines():
                if line.startswith("summary:"):
                    summary = line.strip()
            matched = 0
            total = 0
            if summary:
                # "summary: N/M exact"
                body = summary.split(":", 1)[1].strip()
                left, _, rest = body.partition("/")
                matched = int(left)
                total = int(rest.split()[0])
            record = {
                "mask": mask,
                "matched": matched,
                "total": total,
                "exact": total > 0 and matched == total,
                "summary": summary,
                "returncode": completed.returncode,
            }
            stream.write(json.dumps(record, sort_keys=True) + "\n")
            stream.flush()
            resolved += 1
            exact += int(record["exact"])
            print(
                f"shard {args.shard}/{args.shards} mask {mask} "
                f"{matched}/{total}",
                flush=True,
            )
    print(
        f"shard {args.shard}/{args.shards} done: {exact}/{resolved} masks exact",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
