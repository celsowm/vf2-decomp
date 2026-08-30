#!/usr/bin/env python3
"""Build a queryable guest-i960 frontier from probe evidence.

The frontier unit is a guest address/edge, not host C coverage. This tool
ingests the evidence already produced by the automation layer:

- `explore_state.py` corpus manifests (`manifest.jsonl` with `new_edges`);
- `sweep_state.py` JSONL sweeps (probe final records);
- `trace_case.py` / `vf2probe --trace` JSONL streams (`step` records); and
- compact function/status metadata from `decomp/i960/functions.csv`.

It ranks candidate addresses and edges by measured features only:

- observed execution count (from traces) or witness count (from corpora);
- whether the address lies inside a recovered function range;
- whether a reproducible corpus snapshot exists for an edge; and
- whether execution terminated in unsupported behavior at the address.

Nothing here decides hardware behavior or invents game semantics. The report
is a navigation aid for choosing the next recovery slice; every admitted
branch still requires the standard differential proof.

Aggregation is streaming: memory use depends on distinct guest addresses and
edges, never on trace length.
"""

from __future__ import annotations

import argparse
import bisect
import csv
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Dict, IO, Iterable, Iterator, List, Optional, Tuple

DEFAULT_FUNCTIONS_CSV = "decomp/i960/functions.csv"

RECOVERED_STATUSES = {
    "recovered",
    "recovered-first-dispatch",
    "recovered-observed-branch",
    "recovered-control-block",
    "recovered-rom-anchor",
    "recovered-prefix",
}


def parse_int(value) -> int:
    if isinstance(value, bool):
        raise ValueError("boolean is not an address")
    if isinstance(value, int):
        return value
    return int(str(value), 0)


def hex32(value: int) -> str:
    return f"0x{value & 0xFFFFFFFF:08x}"


class FunctionTable:
    """Recovered/candidate function ranges from decomp/i960/functions.csv."""

    def __init__(self, rows: Iterable[dict]):
        self.starts: List[int] = []
        self.ends: Dict[int, int] = {}
        self.names: Dict[int, str] = {}
        self.statuses: Dict[int, str] = {}
        for row in rows:
            try:
                start = parse_int(row["address"])
            except (KeyError, ValueError):
                continue
            end_text = (row.get("end") or "").strip()
            name = (row.get("name") or "").strip()
            status = (row.get("status") or "").strip()
            if end_text:
                try:
                    end = parse_int(end_text)
                except ValueError:
                    continue
                if end <= start:
                    continue
                # First definition wins; later duplicates are ignored.
                if start not in self.ends:
                    self.starts.append(start)
                    self.ends[start] = end
                    self.names.setdefault(start, name)
                    self.statuses.setdefault(start, status)
            elif start not in self.names:
                # Range-less rows still attribute addresses that start exactly.
                self.names[start] = name
                self.statuses[start] = status
        self.starts.sort()

    @classmethod
    def load(cls, path: Path) -> "FunctionTable":
        with path.open("r", encoding="utf-8", newline="") as stream:
            return cls(csv.DictReader(stream))

    def lookup(self, address: int) -> Tuple[Optional[int], Optional[str], Optional[str]]:
        """Return (function_start, name, status) for the address."""
        index = bisect.bisect_right(self.starts, address)
        for start in reversed(self.starts[:index]):
            end = self.ends.get(start)
            if end is not None and address < end:
                return start, self.names.get(start), self.statuses.get(start)
        if address in self.names and address not in self.ends:
            # Range-less row: attribute only the exact entry address.
            return None, self.names[address], self.statuses[address]
        return None, None, None


class EdgeRecord:
    __slots__ = ("witnesses", "snapshots", "halted_unsupported", "is_call")

    def __init__(self) -> None:
        self.witnesses = 0
        self.snapshots: set = set()
        self.halted_unsupported = 0
        self.is_call = False


class Frontier:
    def __init__(self) -> None:
        self.edges: Dict[Tuple[int, int], EdgeRecord] = {}
        self.address_executions: Counter = Counter()
        self.unsupported_addresses: Counter = Counter()
        self.sources: Counter = Counter()

    # ------------------------------------------------------------------
    # Ingestion
    # ------------------------------------------------------------------
    def ingest_trace(self, path: Path, source_label: str) -> dict:
        """Ingest one vf2probe --trace/--memory-trace JSONL stream."""
        pending_memory: Dict[int, int] = defaultdict(int)
        stats = {"steps": 0, "memory_accesses": 0, "finals": 0}
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
                    ip_before = parse_int(record["ip_before"])
                    ip_after = parse_int(record.get("ip_after", ip_before))
                    is_call_step = bool(record.get("is_call", False))
                    record_edge = self.edges.setdefault(
                        (ip_before, ip_after), EdgeRecord()
                    )
                    record_edge.witnesses += 1
                    if is_call_step:
                        record_edge.is_call = True
                    self.address_executions[ip_before] += 1
                    stats["steps"] += 1
                    hits = pending_memory.pop(parse_int(record["step"]), None)
                    if hits:
                        stats["memory_accesses"] += hits
                        self.address_executions[ip_before] += hits
                elif kind == "memory":
                    pending_memory[parse_int(record["step"])] += 1
                elif kind == "final":
                    stats["finals"] += 1
                    status_text = str(record.get("status", ""))
                    halt_text = str(record.get("halt_reason", ""))
                    if (
                        status_text != "ok"
                        and "unsupported" in status_text
                    ) or halt_text == "unsupported instruction":
                        self.unsupported_addresses[parse_int(record["ip"])] += 1
        self.sources[source_label] += 1
        return stats

    def ingest_corpus_manifest(self, path: Path, source_label: str) -> dict:
        """Ingest an explore_state.py manifest.jsonl.

        Each accepted case contributes its full new-edge list plus a snapshot
        handle that keeps the witness reproducible.
        """
        stats = {"cases": 0, "edges": 0}
        case_dir = path.parent
        for record in _iter_jsonl(path):
            inputs = record.get("inputs") or {}
            snapshots = []
            snapshot_name = record.get("snapshot")
            if snapshot_name:
                snapshots.append(str(Path(snapshot_name).name))
            else:
                case_index = record.get("case")
                if case_index is not None:
                    candidate = case_dir / f"case-{int(case_index):05d}.vf2snap"
                    if candidate.exists():
                        snapshots.append(candidate.name)
            new_edges = record.get("new_edges") or []
            final = record.get("final") or {}
            halted = str(final.get("status", "")) != "ok"
            for edge in new_edges:
                try:
                    source = parse_int(edge["from"])
                    target = parse_int(edge["to"])
                except (KeyError, ValueError, TypeError):
                    continue
                is_call_edge = bool(edge.get("is_call", False))
                edge_record = self.edges.setdefault((source, target), EdgeRecord())
                edge_record.witnesses += 1
                if is_call_edge:
                    edge_record.is_call = True
                for name in snapshots:
                    edge_record.snapshots.add(name)
                if halted:
                    edge_record.halted_unsupported += 1
                stats["edges"] += 1
            stats["cases"] += 1
        self.sources[source_label] += 1
        return stats

    def ingest_sweep(self, path: Path, source_label: str) -> dict:
        """Ingest a sweep_state.py JSONL file (final records only)."""
        stats = {"cases": 0, "unsupported": 0}
        for record in _iter_jsonl(path):
            outcome = record.get("outcome") or {}
            if not outcome:
                continue
            stats["cases"] += 1
            status_text = str(outcome.get("status", ""))
            if "unsupported" in status_text:
                stats["unsupported"] += 1
                self.unsupported_addresses[parse_int(outcome.get("ip", 0))] += 1
        self.sources[source_label] += 1
        return stats

    # ------------------------------------------------------------------
    # Ranking
    # ------------------------------------------------------------------
    def rank_edges(
        self,
        functions: Optional[FunctionTable],
        limit: int,
        exclude_recovered: bool,
    ) -> List[dict]:
        ranked: List[dict] = []
        for (source, target), record in self.edges.items():
            source_fn = functions.lookup(source) if functions else (None, None, None)
            target_fn = functions.lookup(target) if functions else (None, None, None)
            source_status = source_fn[2]
            target_status = target_fn[2]
            source_native = bool(
                source_status and source_status.startswith("recovered")
            )
            target_native = bool(
                target_status and target_status.startswith("recovered")
            )
            if exclude_recovered and source_native and target_native:
                continue
            distance = _boundary_distance(source_fn, target_fn, source, target)
            is_boundary = source_native != target_native
            score = (
                record.witnesses * 4
                + len(record.snapshots) * 8
                + record.halted_unsupported * 16
                + (12 if is_boundary else 0)
                + (4 if not source_native and not target_native else 0)
                + (6 if distance is not None and distance <= 64 else 0)
            )
            is_call = record.is_call or (
                target_fn[0] is not None and target == target_fn[0]
            )
            call_target_name = target_fn[1] if is_call else None
            call_target_status = target_fn[2] if is_call else None

            ranked.append(
                {
                    "from": hex32(source),
                    "to": hex32(target),
                    "witnesses": record.witnesses,
                    "snapshots": sorted(record.snapshots),
                    "unsupported_finals": record.halted_unsupported,
                    "is_call": is_call,
                    "call_target_name": call_target_name,
                    "call_target_status": call_target_status,
                    "from_function": source_fn[1],
                    "from_status": source_fn[2],
                    "to_function": target_fn[1],
                    "to_status": target_fn[2],
                    "boundary_distance": distance,
                    "score": score,
                }
            )
        ranked.sort(key=lambda item: (-item["score"], item["from"], item["to"]))
        return ranked[:limit]

    def top_unsupported(self, limit: int) -> List[dict]:
        items = self.unsupported_addresses.most_common(limit)
        return [{"address": hex32(address), "count": count} for address, count in items]


def _boundary_distance(
    source_fn: Tuple[Optional[int], Optional[str], Optional[str]],
    target_fn: Tuple[Optional[int], Optional[str], Optional[str]],
    source: int,
    target: int,
) -> Optional[int]:
    """Approximate distance from a recovered boundary.

    Non-zero when one endpoint sits inside recovered code and the other does
    not; smaller values are closer to an already-proven corridor. Returns
    None when neither side attributes to a known function.
    """
    source_start = source_fn[0]
    target_start = target_fn[0]
    if source_start is None and target_start is None:
        return None
    if source_start is None or target_start is None:
        anchor = source_start if source_start is not None else target_start
        point = source if source_start is None else target
        return min(abs(point - anchor), 0x10000)
    return min(abs(source - target), 0x10000)


def _iter_jsonl(path: Path) -> Iterator[dict]:
    with path.open("r", encoding="utf-8") as stream:
        for line in stream:
            line = line.strip()
            if not line:
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                continue
            if isinstance(record, dict):
                yield record


def classify_input(path: Path) -> Optional[str]:
    """Peek at the first JSON record to pick an ingester."""
    for record in _iter_jsonl(path):
        kind = record.get("type")
        if kind in {"step", "memory", "final"}:
            return "trace"
        if "new_edges" in record or "inputs" in record:
            return "corpus"
        return None
    return None


def open_output(path: Optional[str]) -> IO:
    if path is None or path == "-":
        return sys.stdout
    return Path(path).open("w", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Rank the guest-i960 recovery frontier from probe corpora, "
            "sweeps and traces"
        )
    )
    parser.add_argument(
        "inputs",
        nargs="+",
        help="manifest.jsonl / sweep JSONL / trace JSONL files",
    )
    parser.add_argument("--functions-csv", default=DEFAULT_FUNCTIONS_CSV)
    parser.add_argument("--limit", type=int, default=40)
    parser.add_argument(
        "--exclude-recovered",
        action="store_true",
        help="hide edges whose endpoints are both inside recovered ranges",
    )
    parser.add_argument("--json", dest="as_json", action="store_true")
    parser.add_argument("--output")
    args = parser.parse_args()

    if args.limit < 1:
        parser.error("--limit must be positive")

    functions_path = Path(args.functions_csv)
    functions = FunctionTable.load(functions_path) if functions_path.exists() else None
    if functions is None:
        print(f"warning: {functions_path} not found; no function attribution",
              file=sys.stderr)

    frontier = Frontier()
    for raw in args.inputs:
        path = Path(raw)
        if not path.exists():
            raise SystemExit(f"input not found: {path}")
        kind = classify_input(path)
        label = path.name
        if kind == "trace":
            stats = frontier.ingest_trace(path, label)
            print(
                f"ingested trace {label}: steps={stats['steps']} "
                f"memory={stats['memory_accesses']}",
                file=sys.stderr,
            )
        elif kind == "corpus":
            stats = frontier.ingest_corpus_manifest(path, label)
            print(
                f"ingested corpus {label}: cases={stats['cases']} "
                f"witnessed edges={stats['edges']}",
                file=sys.stderr,
            )
        elif kind == "sweep":
            stats = frontier.ingest_sweep(path, label)
            print(
                f"ingested sweep {label}: cases={stats['cases']} "
                f"unsupported={stats['unsupported']}",
                file=sys.stderr,
            )
        else:
            print(f"skipping unrecognized input: {path}", file=sys.stderr)

    ranked = frontier.rank_edges(functions, args.limit, args.exclude_recovered)

    output = open_output(args.output)
    try:
        if args.as_json:
            for item in ranked:
                output.write(json.dumps(item, sort_keys=True) + "\n")
        else:
            output.write(
                f"{'edge':<25} {'wit':>5} {'snap':>5} {'unsup':>5} "
                f"{'dist':>6}  function(status)\n"
            )
            for item in ranked:
                edge_type = "CALL" if item["is_call"] else "JUMP"
                edge = f"{item['from']}->{item['to']}"
                where = item["from_function"] or "?"
                status = item["from_status"] or "unknown"
                call_info = ""
                if item["is_call"] and item["call_target_name"]:
                    call_info = f" -> call {item['call_target_name']}({item['call_target_status'] or 'unknown'})"
                output.write(
                    f"{edge:<25} {edge_type:<4} {item['witnesses']:>5} "
                    f"{len(item['snapshots']):>5} "
                    f"{item['unsupported_finals']:>5} "
                    f"{item['boundary_distance'] if item['boundary_distance'] is not None else '-':>6}  "
                    f"{where}({status}){call_info}\n"
                )
        unsupported = frontier.top_unsupported(10)
        if unsupported and not args.as_json:
            output.write("\nunsupported-final addresses:\n")
            for item in unsupported:
                output.write(f"  {item['address']}  x{item['count']}\n")
    finally:
        if output is not sys.stdout:
            output.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
