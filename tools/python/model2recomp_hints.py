#!/usr/bin/env python3
"""Extract optional i960 entry-point hints from model2recomp's static lifter.

This is an analysis-only bridge.  The external checkout stays outside this
repository, and the returned addresses are candidates for inspection with
vf2i960/the reference executor; they are never native recoveries.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Mapping, Sequence


class Model2RecompHintsError(RuntimeError):
    """A configuration or external-analysis failure."""


def _environment(environ: Mapping[str, str] | None) -> Mapping[str, str]:
    return os.environ if environ is None else environ


def resolve_model2recomp_root(
    explicit: Path | None = None, *, environ: Mapping[str, str] | None = None
) -> Path:
    """Resolve the optional model2recomp checkout without a fallback."""

    candidate = explicit
    if candidate is None:
        configured = _environment(environ).get("MODEL2RECOMP_ROOT")
        if configured:
            candidate = Path(configured)
    if candidate is None:
        raise Model2RecompHintsError(
            "model2recomp checkout not configured; pass --model2recomp-root "
            "or set MODEL2RECOMP_ROOT"
        )

    root = candidate.expanduser().resolve()
    lifter = root / "tools" / "i960_lifter.py"
    if not lifter.is_file():
        raise Model2RecompHintsError(
            f"invalid model2recomp checkout {root}: missing "
            f"{lifter.relative_to(root)}"
        )
    return root


def _git_revision(root: Path) -> str | None:
    try:
        result = subprocess.run(
            ["git", "-C", str(root), "rev-parse", "HEAD"],
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except OSError:
        return None
    if result.returncode != 0:
        return None
    revision = result.stdout.strip()
    return revision or None


def _load_lifter(root: Path):
    """Load only the external static-lifter module, never its runtime."""

    module_path = root / "tools" / "i960_lifter.py"
    spec = importlib.util.spec_from_file_location(
        "model2recomp_i960_lifter", module_path
    )
    if spec is None or spec.loader is None:
        raise Model2RecompHintsError(f"could not load {module_path}")
    module = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(module)
    except Exception as exc:  # pragma: no cover - depends on external checkout
        raise Model2RecompHintsError(
            f"could not import model2recomp static lifter: {exc}"
        ) from exc
    return module


def _read_hints(path: Path | None) -> list[int]:
    if path is None:
        return []
    hints: list[int] = []
    for line_number, raw_line in enumerate(
        path.read_text(encoding="utf-8").splitlines(), start=1
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split()
        if len(fields) != 2 or fields[0].lower() != "entry":
            raise Model2RecompHintsError(
                f"invalid hint line {line_number} in {path}: {raw_line!r}"
            )
        try:
            address = int(fields[1], 0)
        except ValueError as exc:
            raise Model2RecompHintsError(
                f"invalid hint address on line {line_number} in {path}: "
                f"{fields[1]!r}"
            ) from exc
        if address < 0 or address & 3:
            raise Model2RecompHintsError(
                f"hint address must be non-negative and 4-byte aligned: "
                f"0x{address:x}"
            )
        hints.append(address)
    return sorted(set(hints))


def _hex_addresses(addresses: Sequence[int]) -> list[str]:
    return [f"0x{address:08x}" for address in sorted(set(addresses))]


def collect_hints(
    *,
    program_bin: Path,
    model2recomp_root: Path,
    data_rom: Path | None = None,
    max_size: int | None = None,
    hints_file: Path | None = None,
) -> dict:
    """Run the external static discovery helpers and return deterministic JSON."""

    program_path = program_bin.expanduser().resolve()
    if not program_path.is_file():
        raise Model2RecompHintsError(f"program binary not found: {program_path}")
    data = program_path.read_bytes()
    limit = len(data) if max_size is None else max_size
    if limit <= 0 or limit > len(data):
        raise Model2RecompHintsError(
            f"max-size must be in the range 1..{len(data)} (received {limit})"
        )

    data_rom_bytes = None
    if data_rom is not None:
        data_rom_path = data_rom.expanduser().resolve()
        if not data_rom_path.is_file():
            raise Model2RecompHintsError(f"data ROM not found: {data_rom_path}")
        data_rom_bytes = data_rom_path.read_bytes()

    hint_addresses = _read_hints(hints_file)
    lifter = _load_lifter(model2recomp_root)
    # i960_lifter imports i960_disasm lazily from its checkout when full
    # discovery runs.  Keep that path scoped to this analysis call, rather
    # than leaking it into the caller's Python process.
    old_path = list(sys.path)
    sys.path.insert(0, str(model2recomp_root / "tools"))
    try:
        try:
            reinit = set(lifter.reinit_entries(data, limit))
            interrupt = set(
                lifter.interrupt_handlers(data, limit, data_rom=data_rom_bytes)
            )
            discovered = set(
                lifter.discover_functions(
                    data,
                    limit,
                    data_rom=data_rom_bytes,
                    hints=hint_addresses,
                )
            )
        except Exception as exc:  # pragma: no cover - external implementation
            raise Model2RecompHintsError(
                f"model2recomp static discovery failed: {exc}"
            ) from exc
    finally:
        sys.path[:] = old_path

    by_source: dict[str, set[int]] = {
        "reinit_iac": reinit,
        "interrupt_table": interrupt,
        "static_discovery": discovered,
        "runtime_hints": {address for address in hint_addresses if address < limit},
    }
    all_addresses = set().union(*by_source.values())
    entries = []
    for address in sorted(all_addresses):
        entries.append(
            {
                "address": f"0x{address:08x}",
                "sources": [
                    source for source, addresses in by_source.items() if address in addresses
                ],
            }
        )

    return {
        "version": 1,
        "tool": "model2recomp_hints",
        "arch": "i960",
        "model2recomp_revision": _git_revision(model2recomp_root),
        "program_size": len(data),
        "max_size": limit,
        "entries": entries,
        "sources": {
            source: _hex_addresses(addresses)
            for source, addresses in by_source.items()
        },
    }


def _parse_positive_int(value: str) -> int:
    try:
        parsed = int(value, 0)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"invalid integer: {value!r}") from exc
    if parsed <= 0:
        raise argparse.ArgumentTypeError("value must be positive")
    return parsed


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Extract model2recomp i960 entry-point hints"
    )
    parser.add_argument("--program-bin", type=Path, required=True)
    parser.add_argument("--model2recomp-root", type=Path, default=None)
    parser.add_argument("--data-rom", type=Path, default=None)
    parser.add_argument("--max-size", type=_parse_positive_int, default=None)
    parser.add_argument("--hints", type=Path, default=None)
    parser.add_argument("--out", type=Path, default=None)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _build_parser().parse_args(argv)
    try:
        root = resolve_model2recomp_root(args.model2recomp_root)
        report = collect_hints(
            program_bin=args.program_bin,
            model2recomp_root=root,
            data_rom=args.data_rom,
            max_size=args.max_size,
            hints_file=args.hints,
        )
    except Model2RecompHintsError as exc:
        print(f"model2recomp_hints: error: {exc}", file=sys.stderr)
        return 2

    encoded = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.out is None:
        print(encoded, end="")
    else:
        output = args.out.expanduser().resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(encoded, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
