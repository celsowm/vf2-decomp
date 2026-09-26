#!/usr/bin/env python3
"""Run the optional generic i960 liftkit against a VF2 disassembly slice.

The repository's ``vf2i960`` binary remains the source of truth for VF2
disassembly.  This adapter only changes the textual listing shape expected by
the external liftkit and keeps every generated artifact below ``out/``.
Generated C is a navigation scaffold, never a recovered implementation.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping, Sequence


class LiftkitVf2Error(RuntimeError):
    """A recoverable command/configuration error in the optional adapter."""


@dataclass(frozen=True)
class DisassemblyRow:
    address: int
    word_count: int
    normalized: str


@dataclass(frozen=True)
class NormalizedDisassembly:
    text: str
    rows: tuple[DisassemblyRow, ...]
    start: int
    end: int

    @property
    def instruction_count(self) -> int:
        return len(self.rows)

    @property
    def byte_length(self) -> int:
        return self.end - self.start


VF2_LINE_RE = re.compile(r"^(?P<address>[0-9A-Fa-f]{8})\s+(?P<body>\S.*)$")
HEX_WORD_RE = re.compile(r"^[0-9A-Fa-f]{8}$")
SAFE_NAME_RE = re.compile(r"^[A-Za-z0-9_.-]+$")


def _parse_address(value: str) -> int:
    try:
        address = int(value, 0)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"invalid address: {value!r}") from exc
    if not 0 <= address <= 0xFFFFFFFF:
        raise argparse.ArgumentTypeError("address must fit in 32 bits")
    return address


def _parse_positive_count(value: str) -> int:
    try:
        count = int(value, 0)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"invalid count: {value!r}") from exc
    if count <= 0:
        raise argparse.ArgumentTypeError("count must be positive")
    return count


def _parse_vf2_line(line: str, line_number: int) -> DisassemblyRow:
    match = VF2_LINE_RE.match(line.rstrip())
    if match is None:
        raise LiftkitVf2Error(
            f"vf2i960 disassembly line {line_number} is not recognized: {line!r}"
        )

    address = int(match.group("address"), 16)
    body = match.group("body")
    tokens = body.split()
    word_count = 0
    while tokens and HEX_WORD_RE.fullmatch(tokens[0]):
        word_count += 1
        tokens.pop(0)
    if word_count == 0:
        raise LiftkitVf2Error(
            f"vf2i960 disassembly line {line_number} has no instruction words"
        )

    # MAME's i960 parser requires a colon after the eight-digit address.  The
    # words and the decoded operand text are intentionally left unchanged.
    normalized = f"{address:08x}: {body}"
    return DisassemblyRow(address, word_count, normalized)


def normalize_vf2_disassembly(
    text: str, *, expected_address: int | None = None
) -> NormalizedDisassembly:
    """Convert vf2i960's listing shape to the external MAME listing shape.

    The local command emits ``ADDRESS  WORDS  MNEMONIC ...`` while liftkit
    parses ``ADDRESS: WORDS MNEMONIC ...``.  No opcode or operand rewriting is
    performed here.
    """

    rows: list[DisassemblyRow] = []
    for line_number, raw_line in enumerate(text.splitlines(), start=1):
        line = raw_line.strip()
        if not line:
            continue
        rows.append(_parse_vf2_line(line, line_number))

    if not rows:
        raise LiftkitVf2Error("vf2i960 produced no disassembly rows")
    if expected_address is not None and rows[0].address != expected_address:
        raise LiftkitVf2Error(
            "vf2i960 returned an unexpected start address: "
            f"0x{rows[0].address:08x} instead of 0x{expected_address:08x}"
        )

    previous = rows[0].address
    for row in rows[1:]:
        if row.address <= previous:
            raise LiftkitVf2Error(
                "vf2i960 disassembly addresses are not strictly increasing: "
                f"0x{previous:08x} then 0x{row.address:08x}"
            )
        previous = row.address

    end = max(row.address + row.word_count * 4 for row in rows)
    if end <= rows[0].address:
        raise LiftkitVf2Error("vf2i960 disassembly has an empty byte span")
    return NormalizedDisassembly(
        text="\n".join(row.normalized for row in rows) + "\n",
        rows=tuple(rows),
        start=rows[0].address,
        end=end,
    )


def _environment(environ: Mapping[str, str] | None) -> Mapping[str, str]:
    return os.environ if environ is None else environ


def resolve_liftkit_root(
    explicit: Path | None = None, *, environ: Mapping[str, str] | None = None
) -> Path:
    """Resolve and validate the external segamodel2-tools checkout."""

    candidate = explicit
    if candidate is None:
        configured = _environment(environ).get("SEGAMODEL2_TOOLS_ROOT")
        if configured:
            candidate = Path(configured)
    if candidate is None:
        raise LiftkitVf2Error(
            "liftkit checkout not configured; pass --liftkit-root or set "
            "SEGAMODEL2_TOOLS_ROOT"
        )

    root = candidate.expanduser().resolve()
    source_root = root / "liftkit" / "src"
    entrypoint = source_root / "liftkit" / "__main__.py"
    if not entrypoint.is_file():
        raise LiftkitVf2Error(
            f"invalid liftkit checkout {root}: missing {entrypoint.relative_to(root)}"
        )
    return root


def resolve_vf2i960(
    explicit: Path | None = None,
    *,
    search_root: Path | None = None,
    path_value: str | None = None,
) -> Path:
    """Resolve the local vf2i960 executable without inventing a fallback."""

    if explicit is not None:
        binary = explicit.expanduser().resolve()
        if not binary.is_file():
            raise LiftkitVf2Error(f"vf2i960 executable not found: {binary}")
        return binary

    root = (search_root or Path.cwd()).resolve()
    candidates = [
        root / "build" / "Debug" / "vf2i960.exe",
        root / "build" / "Debug" / "vf2i960",
        root / "build" / "vf2i960.exe",
        root / "build" / "vf2i960",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate

    path_binary = shutil.which("vf2i960.exe", path=path_value)
    if path_binary is None:
        path_binary = shutil.which("vf2i960", path=path_value)
    if path_binary is not None:
        return Path(path_binary).resolve()

    searched = ", ".join(str(path) for path in candidates)
    raise LiftkitVf2Error(
        "vf2i960 executable not found; pass --vf2i960 or build it first "
        f"(searched: {searched})"
    )


def _validate_name(name: str) -> str:
    if not SAFE_NAME_RE.fullmatch(name):
        raise LiftkitVf2Error(
            "name must contain only letters, digits, '.', '_' or '-' "
            f"(received {name!r})"
        )
    return name


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


def _relative_output(path_value: str | None, run_root: Path) -> str | None:
    if path_value is None:
        return None
    path = Path(path_value).resolve()
    try:
        return path.relative_to(run_root).as_posix()
    except ValueError as exc:
        raise LiftkitVf2Error(
            f"liftkit wrote an output outside the isolated run directory: {path}"
        ) from exc


def _sanitize_liftkit_report(report: dict, run_root: Path) -> dict:
    outputs = report.get("outputs")
    sanitized_outputs = None
    if isinstance(outputs, dict):
        sanitized_outputs = {
            key: _relative_output(value, run_root)
            for key, value in sorted(outputs.items())
        }
    return {
        "name": report.get("name"),
        "arch": report.get("arch"),
        "kind": report.get("kind"),
        "addr": report.get("addr"),
        "length": report.get("length"),
        "slice": _relative_output(report.get("slice"), run_root),
        "outputs": sanitized_outputs,
    }


def _run_checked(
    command: Sequence[str], *, cwd: Path, environ: Mapping[str, str]
) -> subprocess.CompletedProcess[str]:
    try:
        result = subprocess.run(
            list(command),
            cwd=str(cwd),
            env=dict(environ),
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except OSError as exc:
        raise LiftkitVf2Error(f"could not execute {command[0]!r}: {exc}") from exc
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip()
        raise LiftkitVf2Error(
            f"command failed with exit code {result.returncode}: "
            f"{' '.join(command)}{': ' + detail if detail else ''}"
        )
    return result


def run_lift(
    *,
    rom_dir: Path,
    address: int,
    count: int,
    name: str,
    out_dir: Path,
    vf2i960: Path | None = None,
    liftkit_root: Path | None = None,
    environ: Mapping[str, str] | None = None,
    search_root: Path | None = None,
) -> dict:
    """Disassemble and lift one VF2 slice into an isolated output directory."""

    env = dict(_environment(environ))
    name = _validate_name(name)
    resolved_liftkit = resolve_liftkit_root(liftkit_root, environ=env)
    resolved_vf2i960 = resolve_vf2i960(
        vf2i960,
        search_root=search_root,
        path_value=env.get("PATH"),
    )
    resolved_rom_dir = rom_dir.expanduser().resolve()
    if not resolved_rom_dir.is_dir():
        raise LiftkitVf2Error(f"ROM directory not found: {resolved_rom_dir}")

    disasm_result = _run_checked(
        [
            str(resolved_vf2i960),
            "disasm",
            str(resolved_rom_dir),
            f"0x{address:x}",
            str(count),
        ],
        cwd=resolved_vf2i960.parent,
        environ=env,
    )
    normalized = normalize_vf2_disassembly(
        disasm_result.stdout, expected_address=address
    )

    output_root = out_dir.expanduser().resolve()
    run_root = output_root / (
        f"{name}-{normalized.start:08x}-{normalized.byte_length:x}"
    )
    run_root.mkdir(parents=True, exist_ok=True)
    slice_path = run_root / (
        f"maincpu_{normalized.start:06x}_{normalized.byte_length:x}.asm"
    )
    slice_path.write_text(normalized.text, encoding="utf-8", newline="\n")

    liftkit_src = resolved_liftkit / "liftkit" / "src"
    liftkit_env = dict(env)
    old_pythonpath = liftkit_env.get("PYTHONPATH")
    liftkit_env["PYTHONPATH"] = str(liftkit_src) + (
        os.pathsep + old_pythonpath if old_pythonpath else ""
    )
    lift_out = run_root / "lift"
    scaffold_out = run_root / "scaffold"
    lift_result = _run_checked(
        [
            sys.executable,
            "-m",
            "liftkit",
            "lift",
            "--slice",
            str(slice_path),
            "--name",
            name,
            "--out-dir",
            str(lift_out),
            "--src-dir",
            str(scaffold_out),
        ],
        cwd=run_root,
        environ=liftkit_env,
    )
    try:
        external_report = json.loads(lift_result.stdout)
    except json.JSONDecodeError as exc:
        raise LiftkitVf2Error(
            "liftkit completed without a JSON report: "
            f"{lift_result.stdout[:200]!r}"
        ) from exc
    if not isinstance(external_report, dict):
        raise LiftkitVf2Error("liftkit report is not a JSON object")

    manifest = {
        "version": 1,
        "tool": "liftkit_vf2",
        "arch": "i960",
        "name": name,
        "address": f"0x{normalized.start:08x}",
        "requested_instruction_count": count,
        "instruction_count": normalized.instruction_count,
        "byte_length": normalized.byte_length,
        "liftkit_revision": _git_revision(resolved_liftkit),
        "outputs": {
            "slice": slice_path.relative_to(run_root).as_posix(),
            "lift": "lift",
            "scaffold": "scaffold",
        },
        "liftkit": _sanitize_liftkit_report(external_report, run_root),
    }
    manifest_path = run_root / "manifest.json"
    manifest["manifest"] = manifest_path.relative_to(output_root).as_posix()
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    return manifest


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Lift a VF2 i960 disassembly slice with optional liftkit"
    )
    parser.add_argument("--rom-dir", type=Path, required=True)
    parser.add_argument("--vf2i960", type=Path, default=None)
    parser.add_argument("--liftkit-root", type=Path, default=None)
    parser.add_argument("--address", type=_parse_address, required=True)
    parser.add_argument("--count", type=_parse_positive_count, required=True)
    parser.add_argument("--name", default=None)
    parser.add_argument("--out-dir", type=Path, default=Path("out/liftkit"))
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)
    name = args.name or f"vf2_{args.address:08x}"
    try:
        manifest = run_lift(
            rom_dir=args.rom_dir,
            address=args.address,
            count=args.count,
            name=name,
            out_dir=args.out_dir,
            vf2i960=args.vf2i960,
            liftkit_root=args.liftkit_root,
        )
    except LiftkitVf2Error as exc:
        print(f"liftkit_vf2: error: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(manifest, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
