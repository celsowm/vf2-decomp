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


CALL_MNEMONICS = {"call", "callx", "bal", "balx"}


class EdgeRecord:
    __slots__ = (
        "witnesses",
        "snapshots",
        "halted_unsupported",
        "mem_reads",
        "mem_writes",
        "mem_addresses",
        "mem_widths",
        "call_hits",
        "sources",
        "fighter_read_offsets",
        "fighter_write_offsets",
    )

    def __init__(self) -> None:
        self.witnesses = 0
        self.snapshots: set = set()
        self.halted_unsupported = 0
        self.mem_reads = 0
        self.mem_writes = 0
        self.mem_addresses: Counter = Counter()
        self.mem_widths: Counter = Counter()
        self.call_hits = 0
        self.sources: set = set()
        # Fighter-window offsets touched by this edge, kept distinct from
        # the global fighter-offset roll-up so a single edge can surface
        # exactly which fighter fields it depends on.
        self.fighter_read_offsets: Counter = Counter()
        self.fighter_write_offsets: Counter = Counter()


class Frontier:
    def __init__(self) -> None:
        self.edges: Dict[Tuple[int, int], EdgeRecord] = {}
        self.address_executions: Counter = Counter()
        self.address_reads: Counter = Counter()
        self.address_writes: Counter = Counter()
        self.call_targets: Counter = Counter()
        self.unsupported_addresses: Counter = Counter()
        self.sources: Counter = Counter()
        self.fighter_bases: List[int] = []
        self.fighter_window: int = 0x2000
        # (offset) -> Counter of base hits + r/w + ips
        self.fighter_offsets: Dict[int, dict] = {}

    def set_fighter_bases(self, bases: List[int], window: int = 0x2000) -> None:
        self.fighter_bases = list(bases)
        self.fighter_window = window

    def _note_fighter_access(
        self, address: int, kind: str, ip: int, width: int = 0
    ) -> Optional[int]:
        """Record a fighter-window memory access.

        Returns the fighter-relative offset when the access fell inside at
        least one configured fighter window, otherwise ``None``.  The
        per-edge fighter_offset views use this return value to surface
        exactly which fighter fields a specific i960 edge depends on,
        without re-walking the global ``fighter_offsets`` roll-up.
        """
        if not self.fighter_bases:
            return None
        for base in self.fighter_bases:
            if base <= address < base + self.fighter_window:
                off = address - base
                rec = self.fighter_offsets.setdefault(
                    off,
                    {
                        "bases": set(),
                        "reads": 0,
                        "writes": 0,
                        "ips": Counter(),
                        "widths": Counter(),
                    },
                )
                rec["bases"].add(base)
                if kind == "write":
                    rec["writes"] += 1
                else:
                    rec["reads"] += 1
                rec["ips"][ip] += 1
                if width > 0:
                    rec["widths"][width] += 1
                return off
        return None

    def top_fighter_offsets(self, limit: int = 40) -> List[dict]:
        rows = []
        for off, rec in self.fighter_offsets.items():
            rows.append(
                {
                    "offset": hex32(off),
                    "base_count": len(rec["bases"]),
                    "reads": rec["reads"],
                    "writes": rec["writes"],
                    "total": rec["reads"] + rec["writes"],
                    "top_ips": [hex32(ip) for ip, _ in rec["ips"].most_common(4)],
                    "widths": {str(w): c for w, c in rec["widths"].most_common()},
                }
            )
        rows.sort(key=lambda r: (-r["base_count"], -r["total"], r["offset"]))
        return rows[:limit]

    def contiguous_fighter_blocks(
        self,
        width: int,
        min_count: int = 2,
        min_length: int = 3,
        ip_overlap: float = 0.5,
    ) -> List[dict]:
        """Detect contiguous runs of same-width fighter-relative fields.

        A *block* is a sequence of fighter offsets at addresses
        ``base + offset``, ``base + offset + width``, ``base + offset + 2*width``,
        ... where every offset in the run has at least ``min_count``
        accesses AND a fraction of the participating offsets share the
        same guest IPs (the ``ip_overlap`` threshold).

        This is the v2 factory's primary tool for surfacing struct-like
        fields without hand-enumerating the candidate offsets. The
        P1 evidence path (the 0x1680 4B R/W block in the player
        corridor) was first detected by this function on
        ``trace-both.jsonl``.

        Returns a list of blocks, each with:

        - ``offset``: starting fighter-relative offset;
        - ``end_offset``: one past the last offset in the block;
        - ``length``: number of fields in the block;
        - ``byte_size``: ``length * width``;
        - ``width``: dominant access width;
        - ``reads``, ``writes``, ``total`` across the block;
        - ``base_count``: 1 if only one base, 2 if dual-base;
        - ``top_ips``: top-4 IPs touching the block (union);
        - ``ip_overlap``: fraction of fields that share at least one
          of the block's top IPs.

        Blocks shorter than ``min_length`` are filtered out.
        """
        if width not in (1, 2, 4, 8):
            raise ValueError(f"width must be one of 1/2/4/8, got {width}")
        # Bucket offsets by width.
        by_width: Dict[int, dict] = {
            off: rec
            for off, rec in self.fighter_offsets.items()
            if rec["widths"].get(width, 0) > 0
        }
        if not by_width:
            return []
        sorted_offsets = sorted(by_width.keys())
        blocks: List[dict] = []
        # Greedy walk: a block is a maximal run of consecutive offsets
        # spaced by `width`. We scan by stride.
        runs: List[List[int]] = []
        current: List[int] = []
        prev_off: Optional[int] = None
        for off in sorted_offsets:
            if prev_off is not None and off - prev_off == width:
                current.append(off)
            else:
                if current:
                    runs.append(current)
                current = [off]
            prev_off = off
        if current:
            runs.append(current)
        for run in runs:
            if len(run) < min_length:
                continue
            base_ips: Counter = Counter()
            total_reads = 0
            total_writes = 0
            base_count = 0
            fields_with_top_ip = 0
            for off in run:
                rec = by_width[off]
                if rec["reads"] + rec["writes"] < min_count:
                    continue  # Skip under-counted fields; still in run.
                total_reads += rec["reads"]
                total_writes += rec["writes"]
                if base_count == 0:
                    base_count = len(rec["bases"])
                else:
                    base_count = min(base_count, len(rec["bases"]))
                base_ips.update(rec["ips"])
                if any(_ >= 2 for _ in rec["ips"].values()):
                    fields_with_top_ip += 1
            if total_reads + total_writes == 0:
                continue
            top_ips = [hex32(ip) for ip, _ in base_ips.most_common(4)]
            # ip_overlap: fraction of fields in the block whose IP set
            # intersects with at least one of the block's top-4 IPs.
            if top_ips:
                # Use raw top IP count for the overlap fraction.
                top_ip_ints = {ip for ip, _ in base_ips.most_common(4)}
                overlap = 0
                for off in run:
                    rec = by_width[off]
                    # rec["ips"] is a Counter; intersect with the
                    # top-IP set after materialising to a set.
                    if set(rec["ips"].keys()) & top_ip_ints:
                        overlap += 1
                overlap_frac = overlap / len(run)
            else:
                overlap_frac = 0.0
            if overlap_frac < ip_overlap:
                continue
            blocks.append(
                {
                    "offset": hex32(run[0]),
                    "end_offset": hex32(run[-1] + width),
                    "length": len(run),
                    "byte_size": len(run) * width,
                    "width": width,
                    "reads": total_reads,
                    "writes": total_writes,
                    "total": total_reads + total_writes,
                    "base_count": base_count,
                    "top_ips": top_ips,
                    "ip_overlap": round(overlap_frac, 3),
                }
            )
        # Sort by length descending, then by total accesses.
        blocks.sort(key=lambda b: (-b["length"], -b["total"], b["offset"]))
        return blocks

    # ------------------------------------------------------------------
    # Ingestion
    # ------------------------------------------------------------------
    def ingest_trace(self, path: Path, source_label: str) -> dict:
        """Ingest one vf2probe --trace/--memory-trace JSONL stream."""
        pending_memory: Dict[int, List[dict]] = defaultdict(list)
        stats = {"steps": 0, "memory_accesses": 0, "memory_reads": 0, "memory_writes": 0, "finals": 0, "call_edges": 0}
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
                    mnemonic = str(record.get("mnemonic", "")).strip()
                    record_edge = self.edges.setdefault(
                        (ip_before, ip_after), EdgeRecord()
                    )
                    record_edge.witnesses += 1
                    record_edge.sources.add(source_label)
                    self.address_executions[ip_before] += 1
                    if mnemonic in CALL_MNEMONICS:
                        record_edge.call_hits += 1
                        self.call_targets[ip_after] += 1
                        stats["call_edges"] += 1
                    stats["steps"] += 1
                    hits = pending_memory.pop(parse_int(record["step"]), None)
                    if hits:
                        stats["memory_accesses"] += len(hits)
                        self.address_executions[ip_before] += len(hits)
                        for acc in hits:
                            acc_kind = acc.get("kind", "read")
                            acc_addr = acc.get("address", 0)
                            try:
                                acc_addr = parse_int(acc_addr)
                            except Exception:
                                acc_addr = 0
                            try:
                                acc_width = int(acc.get("size", 0))
                            except Exception:
                                acc_width = 0
                            if acc_kind == "write":
                                record_edge.mem_writes += 1
                                self.address_writes[ip_before] += 1
                                stats["memory_writes"] += 1
                            else:
                                record_edge.mem_reads += 1
                                self.address_reads[ip_before] += 1
                                stats["memory_reads"] += 1
                            if acc_width > 0:
                                record_edge.mem_widths[acc_width] += 1
                            if acc_addr:
                                record_edge.mem_addresses[acc_addr] += 1
                                fighter_off = self._note_fighter_access(
                                    acc_addr, acc_kind, ip_before, acc_width
                                )
                                if fighter_off is not None:
                                    if acc_kind == "write":
                                        record_edge.fighter_write_offsets[fighter_off] += 1
                                    else:
                                        record_edge.fighter_read_offsets[fighter_off] += 1
                elif kind == "memory":
                    pending_memory[parse_int(record["step"])].append(
                        {
                            "kind": str(record.get("kind", "read")),
                            "address": record.get("address", 0),
                            "size": record.get("size", 0),
                        }
                    )
                elif kind == "final":
                    stats["finals"] += 1
                    status_text = str(record.get("status", ""))
                    halt_text = str(record.get("halt_reason", ""))
                    if (
                        status_text != "ok"
                        and "unsupported" in status_text
                    ) or halt_text == "unsupported instruction":
                        self.unsupported_addresses[parse_int(record["ip"])] += 1
        # Any orphaned pending memory (no matching step) still counts as global but not edge-attributed
        for remaining in pending_memory.values():
            stats["memory_accesses"] += len(remaining)
            for acc in remaining:
                if str(acc.get("kind")) == "write":
                    stats["memory_writes"] += 1
                else:
                    stats["memory_reads"] += 1
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
                edge_record = self.edges.setdefault((source, target), EdgeRecord())
                edge_record.witnesses += 1
                edge_record.sources.add(source_label)
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
            mem_total = record.mem_reads + record.mem_writes
            fighter_reads = sum(record.fighter_read_offsets.values())
            fighter_writes = sum(record.fighter_write_offsets.values())
            fighter_total = fighter_reads + fighter_writes
            # Fighter-window accesses are the most actionable signal that an
            # edge drives object semantics; surface them as a separate bonus
            # so they break ties among edges that otherwise look identical.
            fighter_bonus = 0
            if fighter_total:
                distinct_offsets = len(
                    set(record.fighter_read_offsets)
                    | set(record.fighter_write_offsets)
                )
                fighter_bonus = min(12, fighter_total) + distinct_offsets
            score = (
                record.witnesses * 4
                + len(record.snapshots) * 8
                + record.halted_unsupported * 16
                + (12 if is_boundary else 0)
                + (4 if not source_native and not target_native else 0)
                + (6 if distance is not None and distance <= 64 else 0)
                + min(mem_total, 16)
                + (8 if record.call_hits else 0)
                + fighter_bonus
            )
            ranked.append(
                {
                    "from": hex32(source),
                    "to": hex32(target),
                    "witnesses": record.witnesses,
                    "snapshots": sorted(record.snapshots),
                    "sources": sorted(record.sources),
                    "unsupported_finals": record.halted_unsupported,
                    "from_function": source_fn[1],
                    "from_status": source_fn[2],
                    "to_function": target_fn[1],
                    "to_status": target_fn[2],
                    "boundary_distance": distance,
                    "score": score,
                    "mem_reads": record.mem_reads,
                    "mem_writes": record.mem_writes,
                    "mem_total": mem_total,
                    "call_hits": record.call_hits,
                    "top_addresses": [hex32(a) for a, _ in record.mem_addresses.most_common(3)],
                    "top_widths": {str(w): c for w, c in record.mem_widths.most_common(4)},
                    "fighter_read_offsets": [
                        hex32(o) for o, _ in record.fighter_read_offsets.most_common(4)
                    ],
                    "fighter_write_offsets": [
                        hex32(o) for o, _ in record.fighter_write_offsets.most_common(4)
                    ],
                    "fighter_access_count": fighter_total,
                }
            )
        ranked.sort(key=lambda item: (-item["score"], item["from"], item["to"]))
        return ranked[:limit]

    def top_unsupported(self, limit: int) -> List[dict]:
        items = self.unsupported_addresses.most_common(limit)
        return [{"address": hex32(address), "count": count} for address, count in items]

    def top_memory_addresses(self, limit: int) -> List[dict]:
        combined = Counter()
        combined.update(self.address_reads)
        combined.update(self.address_writes)
        items = combined.most_common(limit)
        return [
            {
                "address": hex32(addr),
                "reads": self.address_reads.get(addr, 0),
                "writes": self.address_writes.get(addr, 0),
                "total": count,
            }
            for addr, count in items
        ]

    def top_call_targets(self, limit: int) -> List[dict]:
        items = self.call_targets.most_common(limit)
        return [{"address": hex32(addr), "count": count} for addr, count in items]

    def rank_call_edges(
        self,
        functions: Optional["FunctionTable"],
        limit: int,
    ) -> List[dict]:
        """Rank measured call/bal edges with target-function attribution."""
        ranked: List[dict] = []
        for (source, target), record in self.edges.items():
            if record.call_hits <= 0:
                continue
            source_fn = functions.lookup(source) if functions else (None, None, None)
            target_fn = functions.lookup(target) if functions else (None, None, None)
            source_native = (source_fn[2] or "") in RECOVERED_STATUSES
            target_native = (target_fn[2] or "") in RECOVERED_STATUSES
            score = (
                record.call_hits * 4
                + record.witnesses * 2
                + record.halted_unsupported * 16
                + (12 if source_native != target_native else 0)
                + (0 if target_native else 6)
            )
            ranked.append(
                {
                    "from": hex32(source),
                    "to": hex32(target),
                    "call_hits": record.call_hits,
                    "witnesses": record.witnesses,
                    "from_function": source_fn[1],
                    "from_status": source_fn[2],
                    "to_function": target_fn[1],
                    "to_status": target_fn[2],
                    "crosses_boundary": source_native != target_native,
                    "score": score,
                }
            )
        ranked.sort(key=lambda item: (-item["score"], item["from"], item["to"]))
        return ranked[:limit]


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
        if "outcome" in record:
            return "sweep"
        return None
    return None


def export_duckdb(frontier: "Frontier", functions: Optional["FunctionTable"], path: Path) -> None:
    """Persist frontier to DuckDB for large corpora (streaming aggregation stays in Python)."""
    try:
        import duckdb  # type: ignore
    except Exception as exc:
        raise SystemExit(f"duckdb required for --duckdb export: {exc}") from exc

    ranked = frontier.rank_edges(functions, limit=1000000, exclude_recovered=False)
    unsupported = frontier.top_unsupported(100000)
    mem_hot = frontier.top_memory_addresses(1000)
    call_hot = frontier.top_call_targets(1000)

    conn = duckdb.connect(str(path))
    try:
        conn.execute("DROP TABLE IF EXISTS frontier_edges")
        conn.execute(
            """
            CREATE TABLE frontier_edges (
                from_addr TEXT, to_addr TEXT, witnesses BIGINT, snapshots TEXT,
                unsupported_finals BIGINT, from_function TEXT, from_status TEXT,
                to_function TEXT, to_status TEXT, boundary_distance INTEGER,
                score BIGINT, mem_reads BIGINT, mem_writes BIGINT, mem_total BIGINT,
                call_hits BIGINT, top_addresses TEXT
            )
            """
        )
        for item in ranked:
            conn.execute(
                "INSERT INTO frontier_edges VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                [
                    item["from"], item["to"], item["witnesses"],
                    ",".join(item["snapshots"]),
                    item["unsupported_finals"], item["from_function"] or "",
                    item["from_status"] or "", item["to_function"] or "",
                    item["to_status"] or "",
                    item["boundary_distance"] if item["boundary_distance"] is not None else None,
                    item["score"], item["mem_reads"], item["mem_writes"],
                    item["mem_total"], item["call_hits"],
                    ",".join(item["top_addresses"]),
                ],
            )

        conn.execute("DROP TABLE IF EXISTS unsupported_addresses")
        conn.execute("CREATE TABLE unsupported_addresses (address TEXT, count BIGINT)")
        for item in unsupported:
            conn.execute("INSERT INTO unsupported_addresses VALUES (?,?)", [item["address"], item["count"]])

        conn.execute("DROP TABLE IF EXISTS hot_memory")
        conn.execute("CREATE TABLE hot_memory (address TEXT, reads BIGINT, writes BIGINT, total BIGINT)")
        for item in mem_hot:
            conn.execute("INSERT INTO hot_memory VALUES (?,?,?,?)",
                         [item["address"], item["reads"], item["writes"], item["total"]])

        conn.execute("DROP TABLE IF EXISTS hot_calls")
        conn.execute("CREATE TABLE hot_calls (address TEXT, count BIGINT)")
        for item in call_hot:
            conn.execute("INSERT INTO hot_calls VALUES (?,?)", [item["address"], item["count"]])
    finally:
        conn.close()


def export_parquet(frontier: "Frontier", functions: Optional["FunctionTable"], path: Path) -> None:
    """Export frontier edges to Parquet via DuckDB (requires duckdb + pyarrow)."""
    try:
        import duckdb  # type: ignore
    except Exception as exc:
        raise SystemExit(f"duckdb required for --parquet export: {exc}") from exc
    # ensure parent exists
    path.parent.mkdir(parents=True, exist_ok=True)
    # Build temporary DuckDB in memory and export
    ranked = frontier.rank_edges(functions, limit=1000000, exclude_recovered=False)
    if not ranked:
        raise SystemExit("no edges to export to parquet")

    # Use DuckDB COPY to parquet for efficiency
    conn = duckdb.connect()
    try:
        # create temp table from Python data
        import tempfile
        import json as _json
        tmp_json = Path(tempfile.gettempdir()) / "_frontier_parquet.jsonl"
        with tmp_json.open("w", encoding="utf-8") as out:
            for item in ranked:
                # flatten snapshots/top_addresses to string
                flat = dict(item)
                flat["snapshots"] = ",".join(item["snapshots"])
                flat["top_addresses"] = ",".join(item["top_addresses"])
                out.write(_json.dumps(flat) + "\n")
        conn.execute(f"CREATE TABLE tmp_edges AS SELECT * FROM read_json('{tmp_json}', auto_detect=true)")
        conn.execute(f"COPY tmp_edges TO '{path}' (FORMAT PARQUET)")
        tmp_json.unlink(missing_ok=True)
    finally:
        conn.close()


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
    parser.add_argument("--duckdb", help="persist ranked frontier to DuckDB file (e.g. out/frontier.duckdb)")
    parser.add_argument("--parquet", help="export ranked edges to Parquet file (e.g. out/frontier.parquet)")
    parser.add_argument(
        "--fighter-base",
        action="append",
        default=[],
        help="fighter object base for offset clustering (repeatable)",
    )
    parser.add_argument(
        "--fighter-window",
        type=lambda s: int(s, 0),
        default=0x2000,
        help="fighter object window for --fighter-base (default 0x2000)",
    )
    args = parser.parse_args()

    if args.limit < 1:
        parser.error("--limit must be positive")

    functions_path = Path(args.functions_csv)
    functions = FunctionTable.load(functions_path) if functions_path.exists() else None
    if functions is None:
        print(f"warning: {functions_path} not found; no function attribution",
              file=sys.stderr)

    frontier = Frontier()
    fighter_bases = []
    for raw_base in args.fighter_base:
        fighter_bases.append(parse_int(raw_base))
    if fighter_bases:
        frontier.set_fighter_bases(fighter_bases, args.fighter_window)
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

    if args.duckdb:
        export_duckdb(frontier, functions, Path(args.duckdb))
        print(f"wrote DuckDB frontier: {args.duckdb}", file=sys.stderr)
    if args.parquet:
        export_parquet(frontier, functions, Path(args.parquet))
        print(f"wrote Parquet frontier: {args.parquet}", file=sys.stderr)

    output = open_output(args.output)
    try:
        if args.as_json:
            for item in ranked:
                output.write(json.dumps(item, sort_keys=True) + "\n")
        else:
            output.write(
                f"{'edge':<25} {'wit':>5} {'snap':>5} {'unsup':>5} "
                f"{'mem':>5} {'call':>4} {'dist':>6}  {'fighter':>8}  "
                f"{'src':>3}  function(status)\n"
            )
            for item in ranked:
                edge = f"{item['from']}->{item['to']}"
                where = item["from_function"] or "?"
                status = item["from_status"] or "unknown"
                src_count = len(item.get("sources") or [])
                fighter_total = item.get("fighter_access_count") or 0
                output.write(
                    f"{edge:<25} {item['witnesses']:>5} "
                    f"{len(item['snapshots']):>5} "
                    f"{item['unsupported_finals']:>5} "
                    f"{item['mem_total']:>5} "
                    f"{item['call_hits']:>4} "
                    f"{item['boundary_distance'] if item['boundary_distance'] is not None else '-':>6}  "
                    f"{fighter_total:>8}  "
                    f"{src_count:>3}  "
                    f"{where}({status})\n"
                )
        unsupported = frontier.top_unsupported(10)
        if unsupported and not args.as_json:
            output.write("\nunsupported-final addresses:\n")
            for item in unsupported:
                output.write(f"  {item['address']}  x{item['count']}\n")
            mem_top = frontier.top_memory_addresses(8)
            if mem_top:
                output.write("\nhottest memory-access IPs:\n")
                for item in mem_top:
                    output.write(f"  {item['address']}  R:{item['reads']} W:{item['writes']} total:{item['total']}\n")
            call_top = frontier.top_call_targets(8)
            if call_top:
                output.write("\ncall-target IPs:\n")
                for item in call_top:
                    output.write(f"  {item['address']}  x{item['count']}\n")
            call_edges = frontier.rank_call_edges(functions, 12)
            if call_edges:
                output.write("\ncall edges (source -> target):\n")
                for item in call_edges:
                    where = item["from_function"] or "?"
                    target = item["to_function"] or "?"
                    tstatus = item["to_status"] or "unknown"
                    mark = " *" if item["crosses_boundary"] else ""
                    output.write(
                        f"  {item['from']}->{item['to']}  x{item['call_hits']}  "
                        f"{where} -> {target}({tstatus}){mark}\n"
                    )
            fighter_rows = frontier.top_fighter_offsets(20)
            if fighter_rows:
                output.write("\nfighter-relative offsets:\n")
                for item in fighter_rows:
                    ips = ",".join(item["top_ips"])
                    widths = ",".join(
                        f"{w}x{c}" for w, c in item["widths"].items()
                    )
                    output.write(
                        f"  {item['offset']}  bases={item['base_count']} "
                        f"R:{item['reads']} W:{item['writes']} "
                        f"widths:{widths} ips:{ips}\n"
                    )
        if args.as_json and frontier.fighter_offsets:
            for item in frontier.top_fighter_offsets(40):
                output.write(
                    json.dumps({"kind": "fighter_offset", **item}, sort_keys=True)
                    + "\n"
                )
            if frontier.fighter_bases:
                for width in (4, 2, 1):
                    for b in frontier.contiguous_fighter_blocks(
                        width=width, min_count=1, min_length=3,
                    ):
                        output.write(
                            json.dumps(
                                {"kind": "fighter_contiguous_block", **b},
                                sort_keys=True,
                            )
                            + "\n"
                        )
    finally:
        if output is not sys.stdout:
            output.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
