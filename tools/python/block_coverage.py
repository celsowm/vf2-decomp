#!/usr/bin/env python3
"""Per-function block coverage report for the recovery frontier.

Combines two pieces of evidence already on disk:

- ``decomp/i960/functions.csv`` — every recovered, prefix-only or
  candidate function range with its name and status.
- ``out/trace-*.jsonl`` (or any ``step`` JSONL) — the set of guest
  addresses the reference executor actually visited.

For each function with a real (start, end) range the tool computes:

- ``range_bytes`` and ``range_words`` (i960 instructions are 4 bytes);
- ``addresses_in_trace`` — distinct ``ip_before``/``ip_after`` values
  that fell inside the range;
- ``largest_uncovered_run`` — the longest contiguous run of *covered*
  i960 words (4-byte strides) whose every address was either not in
  the trace or not yet assigned a recovered status;
- ``coverage_ratio`` — ``addresses_in_trace / range_words``;
- ``recovered_status`` — one of ``fully_recovered``, ``prefix``,
  ``control_block``, ``prefixes``, ``observed_branch``,
  ``first_dispatch``, ``rom_anchor``, ``unknown`` (parsed from the
  CSV ``status`` column).

The output is sorted by a configurable key (largest_uncovered_run desc
by default) and rendered as both a human-readable text table and a
machine-readable JSONL stream. Functions marked as
``recovered-control-block`` are tagged in a separate ``wrapper_*``
column so the report can distinguish a real function from a wrapper
that spans a large code region.

The tool exists because there is no automated way today to answer the
question *"which ``functions.csv`` entry has the largest unmeasured
gap that the next session should tackle?"* — and that is exactly the
shape of question the v0730 runbook expects the factory chain to
answer for ``P3-P4``.

This tool does **not** invent any game semantics, decide hardware
behaviour, or modify any code. It is a navigation aid only.
"""

from __future__ import annotations

import argparse
import csv
import json
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Set, Tuple


DEFAULT_FUNCTIONS_CSV = "decomp/i960/functions.csv"
DEFAULT_TRACE_GLOB = "out/trace-*.jsonl"
I960_WORD = 4

STATUS_BUCKETS = {
    "recovered": "fully_recovered",
    "recovered-prefix": "prefix",
    "recovered-prefixes": "prefixes",
    "recovered-control-block": "control_block",
    "recovered-first-dispatch": "first_dispatch",
    "recovered-observed-branch": "observed_branch",
    "recovered-rom-anchor": "rom_anchor",
}


@dataclass
class FunctionRange:
    start: int
    end: int
    name: str
    status: str
    notes: str = ""
    is_container: bool = False

    @property
    def byte_size(self) -> int:
        return self.end - self.start

    @property
    def word_count(self) -> int:
        return self.byte_size // I960_WORD

    @property
    def status_bucket(self) -> str:
        return STATUS_BUCKETS.get(self.status, "unknown")

    @property
    def is_wrapper(self) -> bool:
        # A *wrapper* is a control-block range that is large enough
        # to span multiple functions. The CSV has at least one such
        # entry (``main_texture_orchestrator_call`` spans 269 KB).
        # Surface these distinctly so they cannot dominate the report.
        return self.status_bucket == "control_block" and self.byte_size > 0x10000

    @property
    def is_excluded_by_default(self) -> bool:
        # Two different reasons a range cannot be ranked as if it were one
        # function. Both are properties of the CSV rows, not of the game.
        return self.is_wrapper or self.is_container


def mark_containers(ranges: List[FunctionRange]) -> int:
    """Flag rows that strictly contain another row's range.

    ``functions.csv`` is a mix of leaves and containers: a container's
    ``end`` is a region bound rather than the procedure's own extent, so its
    uncovered run is the union of its children's gaps. Ranking by that
    attributes a child's work to its parent, and lets one container dominate
    the whole report - which is exactly what happens without this: the top
    four entries were 45-66 KB "functions" whose own notes say they are
    63-instruction bodies, and each one nested several smaller rows.

    Overlap alone is not the test. A container must *strictly* contain
    another row, so an ordinary prefix/leaf decomposition where siblings only
    abut is left alone. The 20 flagged rows are containers; the remaining 49
    are leaves with plausible 24-892 byte extents.
    """
    marked = 0
    for outer in ranges:
        for inner in ranges:
            if outer is inner:
                continue
            if (outer.start <= inner.start and outer.end >= inner.end
                    and outer.byte_size > inner.byte_size):
                outer.is_container = True
                marked += 1
                break
    return marked


def find_inverted_ranges(path: Path) -> List[Tuple[str, int, int]]:
    """Rows in ``functions.csv`` whose ``end`` precedes ``address``.

    Returns ``(name, start, end)`` triples. These rows are dropped by
    ``load_functions`` (a negative ``byte_size`` does not describe a code
    range), and dropping them *silently* is the defect this function exists
    to expose: before it, 25 of the 94 bounded rows vanished with no warning,
    and a short report is indistinguishable from a well-covered one.

    The scan is deliberately independent of ``load_functions`` so that the
    coverage table keeps its current, tested semantics while the census stays
    honest. Nothing here guesses an extent.

    Why these rows exist: ``functions.csv`` overloads its ``end`` column. Most
    rows carry a code extent (one past the last instruction). A minority carry
    the *call-return continuation* -- the address control resumes at in the
    caller -- which is a different quantity and may sit anywhere in the ROM,
    including below the function's own entry. ``load_functions`` drops them at
    ``end <= start``. They are not corrupt data and must not be "repaired" by
    guessing an extent, but they are not ranges either.
    """
    out: List[Tuple[str, int, int]] = []
    with path.open("r", encoding="utf-8", newline="") as stream:
        for row in csv.DictReader(stream):
            try:
                start = parse_int(row["address"])
            except (KeyError, ValueError):
                continue
            end_text = (row.get("end") or "").strip()
            if not end_text:
                continue
            try:
                end = parse_int(end_text)
            except ValueError:
                continue
            if end < start:
                out.append(((row.get("name") or "").strip(), start, end))
    return out


def parse_int(value) -> int:
    if isinstance(value, bool):
        raise ValueError("boolean is not an address")
    if isinstance(value, int):
        return value
    return int(str(value), 0)


def hex32(value: int) -> str:
    return f"0x{value & 0xFFFFFFFF:08x}"


def load_functions(path: Path) -> List[FunctionRange]:
    out: List[FunctionRange] = []
    with path.open("r", encoding="utf-8", newline="") as stream:
        for row in csv.DictReader(stream):
            try:
                start = parse_int(row["address"])
            except (KeyError, ValueError):
                continue
            end_text = (row.get("end") or "").strip()
            if not end_text:
                continue
            try:
                end = parse_int(end_text)
            except ValueError:
                continue
            if end <= start:
                continue
            out.append(
                FunctionRange(
                    start=start,
                    end=end,
                    name=(row.get("name") or "").strip(),
                    status=(row.get("status") or "").strip(),
                    notes=(row.get("notes") or "").strip(),
                )
            )
    out.sort(key=lambda f: f.start)
    mark_containers(out)
    return out


def collect_trace_addresses(paths: Iterable[Path]) -> Set[int]:
    seen: Set[int] = set()
    for path in paths:
        if not path.exists():
            continue
        with path.open("r", encoding="utf-8") as stream:
            for line in stream:
                line = line.strip()
                if not line:
                    continue
                try:
                    record = json.loads(line)
                except json.JSONDecodeError:
                    continue
                kind = record.get("type")
                if kind == "step":
                    seen.add(parse_int(record["ip_before"]))
                    ip_after = record.get("ip_after")
                    if ip_after is not None:
                        seen.add(parse_int(ip_after))
                elif kind == "memory":
                    addr = record.get("address")
                    if addr is not None:
                        seen.add(parse_int(addr))
    return seen


def longest_uncovered_run(
    func: FunctionRange, addresses: Set[int]
) -> int:
    """Longest contiguous run of *uncovered* i960 words.

    A word at offset ``i`` is covered iff its absolute address is in
    ``addresses``. A run is a maximal sequence of consecutive
    uncovered words, in 4-byte strides. Runs that cross the function
    boundary are clipped.
    """
    if func.byte_size <= 0:
        return 0
    longest = 0
    current = 0
    cur = func.start
    end = func.end
    while cur < end:
        if cur in addresses:
            current = 0
        else:
            current += 1
            if current > longest:
                longest = current
        cur += I960_WORD
    return longest


def total_uncovered_words(func: FunctionRange, addresses: Set[int]) -> int:
    """Number of i960 words in the range that the trace never visited.

    Wrappers and large control blocks can dwarf real functions. The
    tool reports this verbatim so callers can decide how to weight
    it.
    """
    if func.byte_size <= 0:
        return 0
    count = 0
    cur = func.start
    while cur < func.end:
        if cur not in addresses:
            count += 1
        cur += I960_WORD
    return count


def addresses_in_range(func: FunctionRange, addresses: Set[int]) -> int:
    if func.byte_size <= 0:
        return 0
    count = 0
    cur = func.start
    while cur < func.end:
        if cur in addresses:
            count += 1
        cur += I960_WORD
    return count


@dataclass
class FunctionReport:
    name: str
    start: int
    end: int
    status: str
    status_bucket: str
    is_wrapper: bool
    is_container: bool

    @property
    def is_excluded_by_default(self) -> bool:
        return self.is_wrapper or self.is_container
    is_container: bool
    byte_size: int
    word_count: int
    addresses_in_trace: int
    coverage_ratio: float
    total_uncovered_words: int
    largest_uncovered_run: int
    notes: str = ""

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "start": hex32(self.start),
            "end": hex32(self.end),
            "status": self.status,
            "status_bucket": self.status_bucket,
            "is_wrapper": self.is_wrapper,
            "is_container": self.is_container,
            "byte_size": self.byte_size,
            "word_count": self.word_count,
            "addresses_in_trace": self.addresses_in_trace,
            "coverage_ratio": round(self.coverage_ratio, 4),
            "total_uncovered_words": self.total_uncovered_words,
            "largest_uncovered_run": self.largest_uncovered_run,
            "notes": self.notes,
        }


def build_report(
    functions: List[FunctionRange], addresses: Set[int]
) -> List[FunctionReport]:
    rows: List[FunctionReport] = []
    for func in functions:
        if func.byte_size <= 0 or func.word_count <= 0:
            continue
        covered = addresses_in_range(func, addresses)
        uncovered = total_uncovered_words(func, addresses)
        ratio = covered / func.word_count if func.word_count else 0.0
        rows.append(
            FunctionReport(
                name=func.name,
                start=func.start,
                end=func.end,
                status=func.status,
                status_bucket=func.status_bucket,
                is_wrapper=func.is_wrapper,
                is_container=func.is_container,
                byte_size=func.byte_size,
                word_count=func.word_count,
                addresses_in_trace=covered,
                coverage_ratio=ratio,
                total_uncovered_words=uncovered,
                largest_uncovered_run=longest_uncovered_run(func, addresses),
                notes=func.notes,
            )
        )
    return rows


def rank_reports(rows: List[FunctionReport], key: str) -> List[FunctionReport]:
    """Sort rows by the requested key.

    Recognised keys (all descending):

    - ``largest_uncovered_run`` (default) — longest contiguous gap in
      i960 words; this is the most actionable "where is the next slice"
      signal;
    - ``total_uncovered_words`` — total unvisited words in the range;
    - ``coverage_ratio`` — small first (under-tested first);
    - ``byte_size`` — largest ranges first.

    Ties break by code order (start address ascending).
    """
    sort_keys = {
        "largest_uncovered_run": lambda r: r.largest_uncovered_run,
        "total_uncovered_words": lambda r: r.total_uncovered_words,
        "coverage_ratio": lambda r: r.coverage_ratio,
        "byte_size": lambda r: r.byte_size,
    }
    if key not in sort_keys:
        raise ValueError(f"unknown --sort key: {key}")
    # Python's sort is stable; sort by code order first so the second
    # sort produces a stable, descending-by-key view (ties keep CSV
    # order).
    rows.sort(key=lambda r: r.start)
    rows.sort(key=lambda r: -sort_keys[key](r))
    return rows


def render_text(rows: List[FunctionReport]) -> str:
    """Render a human-readable report (text table).

    `rows` arrives already filtered and limited; see the comment at the call
    site for why the order matters.
    """
    lines = [
        f"{'name':<32} {'status':<26} {'size':>7} "
        f"{'in_trace':>9} {'cov':>5} {'uncovered':>10} "
        f"{'long_run':>9}  range"
    ]
    for r in rows:
        lines.append(
            f"{r.name[:32]:<32} {r.status:<26} "
            f"{r.byte_size:>7} "
            f"{r.addresses_in_trace:>9} "
            f"{r.coverage_ratio:>5.2f} "
            f"{r.total_uncovered_words:>10} "
            f"{r.largest_uncovered_run:>9}  "
            f"{hex32(r.start)}..{hex32(r.end)}"
        )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Per-function block coverage report for the recovery "
            "frontier (functions.csv + trace JSONL)."
        )
    )
    parser.add_argument(
        "--functions-csv",
        default=DEFAULT_FUNCTIONS_CSV,
        help="recovered functions table (default: decomp/i960/functions.csv)",
    )
    parser.add_argument(
        "--trace",
        action="append",
        default=[],
        help="trace JSONL file to ingest (repeatable)",
    )
    parser.add_argument(
        "--trace-glob",
        default="",
        help="glob to expand into --trace entries (default: off)",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=40,
        help="maximum number of functions in the report",
    )
    parser.add_argument(
        "--strict-ranges",
        action="store_true",
        help=(
            "exit non-zero if any functions.csv row has end < address. Those "
            "rows record a call-return continuation instead of a code extent "
            "and are excluded from the report; this turns that exclusion into "
            "a hard gate instead of a warning."
        ),
    )
    parser.add_argument(
        "--include-wrappers",
        action="store_true",
        help=(
            "include control-block wrappers and container ranges "
            "(default: excluded). A container's `end` is a region bound, not "
            "the procedure's own extent, so its uncovered run is the union of "
            "its children's gaps and it dominates any ranking by size."
        ),
    )
    parser.add_argument(
        "--sort",
        default="largest_uncovered_run",
        choices=(
            "largest_uncovered_run",
            "total_uncovered_words",
            "coverage_ratio",
            "byte_size",
        ),
        help="sort key (default: largest_uncovered_run)",
    )
    parser.add_argument(
        "--json",
        dest="as_json",
        action="store_true",
        help="emit JSONL records (one per function) instead of text",
    )
    parser.add_argument(
        "--output",
        default="-",
        help="output file (default: stdout)",
    )
    args = parser.parse_args()

    functions_path = Path(args.functions_csv)
    if not functions_path.exists():
        raise SystemExit(f"--functions-csv not found: {functions_path}")
    functions = load_functions(functions_path)

    # A row whose `end` precedes its `address` cannot be ranked as a code
    # range, so build_report() drops it. Name every one of them: a report
    # that silently lost 25 of 94 rows is indistinguishable from a report
    # over a well-covered table.
    inverted = find_inverted_ranges(functions_path)
    if inverted:
        sys.stderr.write(
            f"warning: {len(inverted)} functions.csv rows have end < address "
            f"and are EXCLUDED from this report. Their 'end' column holds a "
            f"call-return continuation, not a code extent:\n"
        )
        for name, start, end in inverted:
            sys.stderr.write(f"  {name[:34]:<34} {hex32(start)}..{hex32(end)}\n")
        if args.strict_ranges:
            raise SystemExit(
                f"--strict-ranges: {len(inverted)} inverted row(s) in "
                f"{functions_path}"
            )

    trace_paths: List[Path] = []
    if args.trace_glob:
        for raw in sorted(Path().glob(args.trace_glob)):
            if raw.is_file():
                trace_paths.append(raw)
    for raw in args.trace:
        p = Path(raw)
        if not p.exists():
            raise SystemExit(f"--trace not found: {p}")
        trace_paths.append(p)
    if not trace_paths:
        print(
            "warning: no traces supplied; every function will report "
            "0 addresses_in_trace",
            file=sys.stderr,
        )
    addresses = collect_trace_addresses(trace_paths)
    rows = build_report(functions, addresses)
    rows = rank_reports(rows, args.sort)
    if args.limit < 1:
        parser.error("--limit must be positive")
    # Exclude, THEN limit. Doing it the other way round returns an empty
    # report whenever the top-N by uncovered run happen to be all containers -
    # which is exactly the case here, and an empty table is indistinguishable
    # from "the corpus covered everything". Applying the filter here also keeps
    # the JSON and text outputs in agreement; previously JSON still emitted the
    # container rows that the text report hid.
    if not args.include_wrappers:
        rows = [r for r in rows if not r.is_excluded_by_default]
    rows = rows[: args.limit]

    if args.as_json:
        out = sys.stdout if args.output == "-" else Path(args.output).open("w", encoding="utf-8")
        try:
            for r in rows:
                out.write(json.dumps(r.to_dict(), sort_keys=True) + "\n")
        finally:
            if out is not sys.stdout:
                out.close()
    else:
        text = render_text(rows)
        if inverted:
            text += (
                f"\n{len(inverted)} bounded functions.csv rows are EXCLUDED "
                f"from this table because their end < address (call-return "
                f"continuation, not a code extent):\n"
            )
            text += "".join(
                f"  {name[:34]:<34} {hex32(start)}..{hex32(end)}\n"
                for name, start, end in inverted
            )
        if args.output == "-":
            sys.stdout.write(text)
        else:
            Path(args.output).write_text(text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())