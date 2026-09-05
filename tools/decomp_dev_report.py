#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path

REPORT_VERSION = 2
FULL_RECOVERY_STATUSES = {"recovered", "recovered-rom-anchor"}
OPEN_STATUSES = {"candidate"}


def parse_int(value: str) -> int:
    return int(value, 0)


def percent(part: int, total: int) -> float:
    return 0.0 if total == 0 else (100.0 * part / total)


def measures(*, total_code: int, matched_code: int, complete_code: int,
             total_functions: int = 0, matched_functions: int = 0,
             total_units: int = 0, complete_units: int = 0) -> dict:
    return {
        "fuzzy_match_percent": percent(matched_code, total_code),
        "total_code": str(total_code),
        "matched_code": str(matched_code),
        "matched_code_percent": percent(matched_code, total_code),
        "total_functions": total_functions,
        "matched_functions": matched_functions,
        "matched_functions_percent": percent(matched_functions, total_functions),
        "complete_code": str(complete_code),
        "complete_code_percent": percent(complete_code, total_code),
        "total_units": total_units,
        "complete_units": complete_units,
    }


def category_for(name: str) -> tuple[str, str]:
    lowered = name.lower()
    if lowered.startswith(("boot", "iac_")):
        return "boot", "Boot / initialization"
    if lowered.startswith(("camera",)):
        return "camera", "Camera"
    if lowered.startswith(("task_sound", "sound", "scsp", "audio")):
        return "audio", "Audio"
    if lowered.startswith(("task_kill_osage", "kill_osage", "task_osage", "osage")):
        return "objects", "Objects / osage"
    if lowered.startswith(("video", "color", "texture", "frame", "palette", "geometry", "tile")):
        return "graphics", "Video / graphics"
    if lowered.startswith(("task_registry", "scheduler", "task_user")):
        return "scheduler", "Scheduler / tasks"
    if lowered.startswith(("game", "input", "player", "fighter", "coli", "collision")):
        return "gameplay", "Gameplay / input / collision"
    if lowered.startswith(("object", "task_object")):
        return "objects", "Objects / osage"
    return "runtime", "Runtime / other"


def merge_ranges(ranges: list[tuple[int, int]]) -> list[tuple[int, int]]:
    merged: list[list[int]] = []
    for start, end in sorted(ranges):
        if not merged or start > merged[-1][1]:
            merged.append([start, end])
        elif end > merged[-1][1]:
            merged[-1][1] = end
    return [(start, end) for start, end in merged]


def range_bytes(ranges: list[tuple[int, int]]) -> int:
    return sum(end - start for start, end in merge_ranges(ranges))


def maincpu_payload_bytes(rom_manifest: Path) -> int:
    total = 0
    with rom_manifest.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            if row["region"] == "maincpu":
                total += parse_int(row["size"])
    if total <= 0:
        raise ValueError("no maincpu ROM payload found")
    return total


def load_units(functions_csv: Path) -> tuple[list[dict], list[tuple[int, int]], list[tuple[int, int]]]:
    units: list[dict] = []
    recovered_ranges: list[tuple[int, int]] = []
    tracked_ranges: list[tuple[int, int]] = []

    with functions_csv.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            status = row["status"].strip()
            end_text = row["end"].strip()
            if status not in FULL_RECOVERY_STATUSES | OPEN_STATUSES or not end_text:
                continue

            start = parse_int(row["address"])
            end = parse_int(end_text)
            if end <= start:
                continue

            size = end - start
            complete = status in FULL_RECOVERY_STATUSES
            category_id, category_name = category_for(row["name"])
            tracked_ranges.append((start, end))
            if complete:
                recovered_ranges.append((start, end))

            unit_measures = measures(
                total_code=size,
                matched_code=size if complete else 0,
                complete_code=size if complete else 0,
                total_functions=1,
                matched_functions=1 if complete else 0,
                total_units=1,
                complete_units=1 if complete else 0,
            )
            units.append({
                "name": row["name"],
                "measures": unit_measures,
                "functions": [{
                    "name": row["name"],
                    "size": str(size),
                    "fuzzy_match_percent": 100.0 if complete else 0.0,
                    "address": str(start),
                    "metadata": {"virtual_address": str(start)},
                }],
                "metadata": {
                    "complete": complete,
                    "source_path": "decomp/i960/functions.csv",
                    "progress_categories": [category_id],
                    "auto_generated": True,
                },
                "_category_name": category_name,
                "_category_id": category_id,
            })

    return units, recovered_ranges, tracked_ranges


def build_report(functions_csv: Path, rom_manifest: Path) -> dict:
    program_bytes = maincpu_payload_bytes(rom_manifest)
    units, recovered_ranges, tracked_ranges = load_units(functions_csv)
    recovered_bytes = range_bytes(recovered_ranges)
    tracked_bytes = range_bytes(tracked_ranges)

    if tracked_bytes > program_bytes or recovered_bytes > tracked_bytes:
        raise ValueError("tracked coverage exceeds maincpu ROM payload")

    remaining = program_bytes - tracked_bytes
    units.append({
        "name": "unrecovered_or_partial_program_rom",
        "measures": measures(total_code=remaining, matched_code=0, complete_code=0, total_units=1),
        "metadata": {
            "complete": False,
            "progress_categories": ["unrecovered"],
            "auto_generated": True,
        },
        "_category_name": "Unrecovered / partial program ROM",
        "_category_id": "unrecovered",
    })

    category_acc: dict[str, dict] = defaultdict(lambda: {
        "name": "",
        "total_code": 0,
        "matched_code": 0,
        "complete_code": 0,
        "total_functions": 0,
        "matched_functions": 0,
        "total_units": 0,
        "complete_units": 0,
    })

    for unit in units:
        cid = unit.pop("_category_id")
        cname = unit.pop("_category_name")
        acc = category_acc[cid]
        acc["name"] = cname
        m = unit["measures"]
        acc["total_code"] += int(m["total_code"])
        acc["matched_code"] += int(m["matched_code"])
        acc["complete_code"] += int(m["complete_code"])
        acc["total_functions"] += m.get("total_functions", 0)
        acc["matched_functions"] += m.get("matched_functions", 0)
        acc["total_units"] += m.get("total_units", 0)
        acc["complete_units"] += m.get("complete_units", 0)

    categories = []
    for cid in sorted(category_acc):
        acc = category_acc[cid]
        categories.append({
            "id": cid,
            "name": acc["name"],
            "measures": measures(
                total_code=acc["total_code"],
                matched_code=acc["matched_code"],
                complete_code=acc["complete_code"],
                total_functions=acc["total_functions"],
                matched_functions=acc["matched_functions"],
                total_units=acc["total_units"],
                complete_units=acc["complete_units"],
            ),
        })

    total_functions = sum(u["measures"].get("total_functions", 0) for u in units)
    matched_functions = sum(u["measures"].get("matched_functions", 0) for u in units)
    complete_units = sum(u["measures"].get("complete_units", 0) for u in units)

    return {
        "measures": measures(
            total_code=program_bytes,
            matched_code=recovered_bytes,
            complete_code=recovered_bytes,
            total_functions=total_functions,
            matched_functions=matched_functions,
            total_units=len(units),
            complete_units=complete_units,
        ),
        "units": units,
        "version": REPORT_VERSION,
        "categories": categories,
    }


def validate_report(report: dict) -> None:
    if report.get("version") != REPORT_VERSION:
        raise ValueError("report version must be 2")
    measures_obj = report.get("measures", {})
    if int(measures_obj.get("total_code", "0")) <= 0:
        raise ValueError("report total_code must be positive")
    if int(measures_obj.get("matched_code", "0")) > int(measures_obj["total_code"]):
        raise ValueError("matched_code exceeds total_code")
    for key in ("total_code", "matched_code", "complete_code"):
        if not isinstance(measures_obj.get(key), str):
            raise ValueError(f"proto3 uint64 field {key} must be JSON string")
    if not report.get("units"):
        raise ValueError("report must contain at least one unit")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate decomp.dev / objdiff Report v2 for VF2 i960 recovery")
    parser.add_argument("--functions", type=Path, default=Path("decomp/i960/functions.csv"))
    parser.add_argument("--rom-manifest", type=Path, default=Path("config/vf2_v22_roms.csv"))
    parser.add_argument("--output", type=Path, default=Path("build/decomp-dev/report.json"))
    parser.add_argument("--check", action="store_true", help="validate inputs/report without writing output")
    args = parser.parse_args()

    report = build_report(args.functions, args.rom_manifest)
    validate_report(report)
    m = report["measures"]
    print(
        f"decomp.dev VF2 2.1: {m['matched_code']}/{m['total_code']} program-ROM bytes "
        f"({m['matched_code_percent']:.4f}%), {m['matched_functions']}/{m['total_functions']} fully tracked functions"
    )
    if not args.check:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2, sort_keys=False) + "\n", encoding="utf-8")
        print(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
