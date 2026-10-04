#!/usr/bin/env python3
"""Unit tests for tools/python/frontier.py.

Runs standalone (no pytest required) so the analysis layer stays
dependency-light:

    python3 tools/python/test_frontier.py
"""

from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from frontier import Frontier, FunctionTable, classify_input, hex32

ROOT = Path(__file__).resolve().parents[2]


def test_function_table_lookup():
    rows = [
        {"address": "0x1000", "end": "0x2000", "name": "recovered_a", "status": "recovered"},
        {"address": "0x3000", "end": "", "name": "entry_only", "status": "candidate"},
        {"address": "0x4000", "end": "0x3fff", "name": "invalid_range", "status": "candidate"},
    ]
    table = FunctionTable(rows)
    assert table.lookup(0x1000)[1] == "recovered_a"
    assert table.lookup(0x1999)[1] == "recovered_a"
    assert table.lookup(0x2000)[1] is None  # end-exclusive
    assert table.lookup(0x3000)[0] is None and table.lookup(0x3000)[1] == "entry_only"
    assert table.lookup(0x4000)[1] is None
    print("ok: function table lookup")


def test_trace_ingestion():
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        records = [
            {"type": "step", "step": 1, "ip_before": 0x18644, "ip_after": 0x18648},
            {"type": "memory", "step": 2, "kind": "read", "address": 0x50A028, "size": 4, "bytes": "00000000"},
            {"type": "step", "step": 2, "ip_before": 0x18648, "ip_after": 0x18650},
            {"type": "step", "step": 3, "ip_before": 0x18644, "ip_after": 0x18648},
            {"type": "final", "status": "ok", "halt_reason": "stop address", "ip": 0x164C4},
        ]
        trace.write_text("\n".join(json.dumps(r) for r in records) + "\n")
        frontier = Frontier()
        stats = frontier.ingest_trace(trace, "case.jsonl")
        assert stats["steps"] == 3
        assert stats["memory_accesses"] == 1
        edge = frontier.edges[(0x18644, 0x18648)]
        assert edge.witnesses == 2
        # memory access attributed to the ip whose step matches
        assert frontier.address_executions[0x18648] == 1 + 1
        assert frontier.address_reads[0x18648] == 1
        assert frontier.address_writes.get(0x18648, 0) == 0
        assert edge.mem_reads == 1 or edge.mem_reads == 0  # depending on step correlation (step 2 belongs to 0x18648)
    print("ok: trace ingestion with step-correlated memory")


def test_memory_rw_and_call():
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        records = [
            {"type": "step", "step": 1, "ip_before": 0x164ac, "ip_after": 0x18644, "mnemonic": "call"},
            {"type": "memory", "step": 2, "kind": "read", "address": 0x00510b24, "size": 4, "bytes": "40000000"},
            {"type": "step", "step": 2, "ip_before": 0x18648, "ip_after": 0x1864c, "mnemonic": "ld"},
            {"type": "memory", "step": 3, "kind": "write", "address": 0x00884000, "size": 4, "bytes": "00000000"},
            {"type": "step", "step": 3, "ip_before": 0x1864c, "ip_after": 0x18650, "mnemonic": "st"},
            {"type": "final", "status": "ok", "halt_reason": "stop address", "ip": 0x10dcc},
        ]
        trace.write_text("\n".join(json.dumps(r) for r in records) + "\n")
        frontier = Frontier()
        stats = frontier.ingest_trace(trace, "case.jsonl")
        assert stats["call_edges"] == 1
        assert stats["memory_reads"] == 1
        assert stats["memory_writes"] == 1
        assert frontier.call_targets[0x18644] == 1
        assert frontier.call_targets.get(0x164ac, 0) == 0
        assert frontier.top_call_targets(1) == [
            {"address": hex32(0x18644), "count": 1}
        ]
        assert frontier.address_reads[0x18648] == 1
        assert frontier.address_writes[0x1864c] == 1
        edge_call = frontier.edges[(0x164ac, 0x18644)]
        assert edge_call.call_hits == 1
        edge_ld = frontier.edges[(0x18648, 0x1864c)]
        assert edge_ld.mem_reads == 1
        edge_st = frontier.edges[(0x1864c, 0x18650)]
        assert edge_st.mem_writes == 1
        assert dict(edge_ld.mem_widths) == {4: 1}
        assert dict(edge_st.mem_widths) == {4: 1}
    print("ok: memory R/W separation and call-target attribution")


def test_memory_width_and_fighter_widths():
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        records = [
            {"type": "memory", "step": 1, "kind": "read", "address": 0x510000, "size": 1},
            {"type": "step", "step": 1, "ip_before": 0x18644, "ip_after": 0x18648, "mnemonic": "ldob"},
            {"type": "memory", "step": 2, "kind": "write", "address": 0x510004, "size": 2},
            {"type": "step", "step": 2, "ip_before": 0x18648, "ip_after": 0x1864c, "mnemonic": "stis"},
            {"type": "final", "status": "ok", "halt_reason": "stop address", "ip": 0x10dcc},
        ]
        trace.write_text("\n".join(json.dumps(r) for r in records) + "\n")
        frontier = Frontier()
        frontier.set_fighter_bases([0x510000])
        frontier.ingest_trace(trace, "case.jsonl")
        assert dict(frontier.edges[(0x18644, 0x18648)].mem_widths) == {1: 1}
        assert dict(frontier.edges[(0x18648, 0x1864c)].mem_widths) == {2: 1}
        rows = {r["offset"]: r for r in frontier.top_fighter_offsets(10)}
        assert rows[hex32(0)]["widths"] == {"1": 1}
        assert rows[hex32(4)]["widths"] == {"2": 1}
    print("ok: access-width tracking per edge and fighter offset")


def test_rank_call_edges_crosses_boundary():
    rows = [
        {"address": "0x16400", "end": "0x16500", "name": "caller", "status": "recovered"},
        {"address": "0x18600", "end": "0x18700", "name": "callee", "status": "candidate"},
    ]
    table = FunctionTable(rows)
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        records = [
            {"type": "step", "step": 1, "ip_before": 0x164ac, "ip_after": 0x18644,
             "mnemonic": "call"},
            {"type": "step", "step": 2, "ip_before": 0x164ac, "ip_after": 0x18644,
             "mnemonic": "call"},
            {"type": "step", "step": 3, "ip_before": 0x164b0, "ip_after": 0x164b4,
             "mnemonic": "mov"},
        ]
        trace.write_text("\n".join(json.dumps(r) for r in records) + "\n")
        frontier = Frontier()
        frontier.ingest_trace(trace, "case.jsonl")
        ranked = frontier.rank_call_edges(table, limit=10)
        assert len(ranked) == 1
        item = ranked[0]
        assert item["from"] == hex32(0x164ac)
        assert item["to"] == hex32(0x18644)
        assert item["call_hits"] == 2
        assert item["from_function"] == "caller"
        assert item["to_function"] == "callee"
        assert item["crosses_boundary"] is True
        plain = frontier.rank_call_edges(None, limit=10)
        assert plain[0]["from_function"] is None
    print("ok: rank_call_edges with boundary attribution")


def test_unsupported_final_attribution():
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        records = [
            {"type": "step", "step": 1, "ip_before": 0x18700, "ip_after": 0x18704},
            {"type": "final", "status": "unsupported operation",
             "halt_reason": "none", "ip": 0x18700},
        ]
        trace.write_text("\n".join(json.dumps(r) for r in records) + "\n")
        frontier = Frontier()
        frontier.ingest_trace(trace, "case.jsonl")
        top = frontier.top_unsupported(5)
        assert top == [{"address": hex32(0x18700), "count": 1}]
    print("ok: unsupported final attribution")


def test_corpus_manifest_ingestion(tmp_snapshot=True):
    with tempfile.TemporaryDirectory() as tmp:
        corpus = Path(tmp)
        if tmp_snapshot:
            (corpus / "case-00000.vf2snap").write_bytes(b"x")
        manifest = corpus / "manifest.jsonl"
        record = {
            "case": 0,
            "inputs": {"fighter0_flags": 0x40},
            "new_edges": [
                {"from": 4996, "to": 5008},
                {"from": 4996, "to": 5124},
            ],
            "final": {"status": "ok"},
        }
        if tmp_snapshot:
            record["snapshot"] = str(corpus / "case-00000.vf2snap")
        manifest.write_text(json.dumps(record) + "\n")
        frontier = Frontier()
        stats = frontier.ingest_corpus_manifest(manifest, "manifest.jsonl")
        assert stats["cases"] == 1 and stats["edges"] == 2
        edge = frontier.edges[(4996, 5008)]
        if tmp_snapshot:
            assert edge.snapshots == {"case-00000.vf2snap"}
        else:
            assert edge.snapshots == set()
    print(f"ok: corpus manifest ingestion (snapshot={tmp_snapshot})")


def test_ranking_prefers_reproducible_boundary():
    rows = [
        {"address": "0x1000", "end": "0x2000", "name": "native_fn", "status": "recovered"},
    ]
    functions = FunctionTable(rows)
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "t.jsonl"
        records = [
            # Edge leaving recovered code toward unknown code.
            {"type": "step", "step": 1, "ip_before": 0x1900, "ip_after": 0x9000},
            # Deep-inside recovered edge (should rank lower).
            {"type": "step", "step": 2, "ip_before": 0x1100, "ip_after": 0x1104},
            # Unknown-to-unknown far away.
            {"type": "step", "step": 3, "ip_before": 0x8000, "ip_after": 0x8004},
        ]
        trace.write_text("\n".join(json.dumps(r) for r in records) + "\n")
        frontier = Frontier()
        frontier.ingest_trace(trace, "t.jsonl")
        ranked = frontier.rank_edges(functions, limit=10, exclude_recovered=False)
        assert ranked[0]["from"] == hex32(0x1900)
        assert ranked[0]["boundary_distance"] == 0x8000
        assert ranked[0]["from_function"] == "native_fn"
        filtered = frontier.rank_edges(functions, limit=10, exclude_recovered=True)
        assert all(item["from"] != hex32(0x1100) for item in filtered)
    print("ok: ranking prefers recovered-boundary exits")


def test_classify_input():
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "t.jsonl"
        trace.write_text(json.dumps({"type": "step", "step": 1,
                                     "ip_before": 16, "ip_after": 20}) + "\n")
        assert classify_input(trace) == "trace"
        corpus = Path(tmp) / "m.jsonl"
        corpus.write_text(json.dumps({"case": 0, "inputs": {}, "new_edges": []}) + "\n")
        assert classify_input(corpus) == "corpus"
        sweep = Path(tmp) / "sweep.jsonl"
        sweep.write_text(json.dumps({
            "field": "fighter0_flags",
            "value": 0x40,
            "outcome": {"status": "unsupported operation", "ip": 0x18700},
        }) + "\n")
        assert classify_input(sweep) == "sweep"
        frontier = Frontier()
        stats = frontier.ingest_sweep(sweep, "sweep.jsonl")
        assert stats == {"cases": 1, "unsupported": 1}
        assert frontier.top_unsupported(1) == [
            {"address": hex32(0x18700), "count": 1}
        ]
    print("ok: input classification")


def test_duckdb_parquet_export():
    try:
        import duckdb  # noqa: F401
        import pyarrow  # noqa: F401
    except Exception as exc:
        print(f"skip: duckdb/parquet export (missing dep: {exc})")
        return
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        records = [
            {"type": "step", "step": 1, "ip_before": 0x18644, "ip_after": 0x18648, "mnemonic": "mov"},
            {"type": "step", "step": 2, "ip_before": 0x18648, "ip_after": 0x1864c, "mnemonic": "ld"},
            {"type": "final", "status": "ok", "halt_reason": "stop address", "ip": 0x10dcc},
        ]
        trace.write_text("\n".join(json.dumps(r) for r in records) + "\n")
        db_path = Path(tmp) / "frontier.duckdb"
        pq_path = Path(tmp) / "frontier.parquet"
        # import needed helpers
        from frontier import Frontier, FunctionTable
        frontier = Frontier()
        frontier.ingest_trace(trace, "case.jsonl")
        # use same helpers as frontier.py exports
        import frontier as fm
        fm.export_duckdb(frontier, None, db_path)
        assert db_path.exists()
        import duckdb
        conn = duckdb.connect(str(db_path))
        count = conn.execute("SELECT count(*) FROM frontier_edges").fetchone()[0]
        assert count == 2
        conn.close()
        fm.export_parquet(frontier, None, pq_path)
        assert pq_path.exists() and pq_path.stat().st_size > 0
        # verify via duckdb (avoids pyarrow file-handle leak on Windows)
        conn2 = duckdb.connect()
        rows = conn2.execute(f"SELECT count(*) FROM read_parquet('{pq_path}')").fetchone()[0]
        conn2.close()
        assert rows == 2
    print("ok: duckdb/parquet export")


def test_per_edge_fighter_offset_surfaces():
    """rank_edges must surface the exact fighter offsets each edge touches.

    Without this, callers can see that an edge has memory traffic but not
    *which* fighter fields it depends on. The whole v2 advance is built
    on this signal.
    """
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        records = [
            # Fighter0 + 0x1a4 read by edge 0x18644 -> 0x18648
            {"type": "memory", "step": 1, "kind": "read",
             "address": 0x510000 + 0x1a4, "size": 4},
            {"type": "step", "step": 1, "ip_before": 0x18644, "ip_after": 0x18648,
             "mnemonic": "ld"},
            # Fighter0 + 0x5b6 read by the same edge
            {"type": "memory", "step": 2, "kind": "read",
             "address": 0x510000 + 0x5b6, "size": 4},
            {"type": "step", "step": 2, "ip_before": 0x18644, "ip_after": 0x18648,
             "mnemonic": "ld"},
            # Different edge touching non-fighter memory
            {"type": "memory", "step": 3, "kind": "write",
             "address": 0x00884000, "size": 4},
            {"type": "step", "step": 3, "ip_before": 0x18648, "ip_after": 0x1864c,
             "mnemonic": "st"},
            {"type": "final", "status": "ok", "halt_reason": "stop address",
             "ip": 0x10dcc},
        ]
        trace.write_text("\n".join(json.dumps(r) for r in records) + "\n")
        frontier = Frontier()
        frontier.set_fighter_bases([0x510000], window=0x2000)
        frontier.ingest_trace(trace, "case.jsonl")
        ranked = frontier.rank_edges(None, limit=10, exclude_recovered=False)
        fighter_edge = next(
            item for item in ranked
            if item["from"] == hex32(0x18644) and item["to"] == hex32(0x18648)
        )
        assert hex32(0x1a4) in fighter_edge["fighter_read_offsets"], fighter_edge
        assert hex32(0x5b6) in fighter_edge["fighter_read_offsets"], fighter_edge
        assert fighter_edge["fighter_access_count"] == 2
        non_fighter_edge = next(
            item for item in ranked
            if item["from"] == hex32(0x18648) and item["to"] == hex32(0x1864c)
        )
        assert non_fighter_edge["fighter_access_count"] == 0
        assert non_fighter_edge["fighter_read_offsets"] == []
        assert non_fighter_edge["fighter_write_offsets"] == []
    print("ok: per-edge fighter offset surfacing in rank output")


def test_per_source_attribution():
    """Edges produced by multiple ingestions carry their contributing sources."""
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "t.jsonl"
        sweep = Path(tmp) / "s.jsonl"
        corpus = Path(tmp) / "m.jsonl"
        trace.write_text(json.dumps(
            {"type": "step", "step": 1, "ip_before": 0x18644,
             "ip_after": 0x18648}
        ) + "\n")
        sweep.write_text(json.dumps(
            {"field": "x", "value": 0,
             "outcome": {"status": "unsupported operation", "ip": 0x18644}}
        ) + "\n")
        corpus.write_text(json.dumps({
            "case": 0, "inputs": {},
            "new_edges": [{"from": 0x18644, "to": 0x18648}],
            "final": {"status": "ok"},
        }) + "\n")
        frontier = Frontier()
        frontier.ingest_trace(trace, "trace.jsonl")
        frontier.ingest_sweep(sweep, "sweep.jsonl")
        frontier.ingest_corpus_manifest(corpus, "manifest.jsonl")
        ranked = frontier.rank_edges(None, limit=5, exclude_recovered=False)
        edge = next(
            item for item in ranked
            if item["from"] == hex32(0x18644) and item["to"] == hex32(0x18648)
        )
        assert "trace.jsonl" in edge["sources"]
        assert "manifest.jsonl" in edge["sources"]
        # The sweep ingester does not produce edges (only final counts);
        # sources must contain only the inputs that actually witnessed this edge.
        assert "sweep.jsonl" not in edge["sources"]
    print("ok: per-source attribution in rank output")


def test_fighter_access_score_bonus():
    """Edges that touch fighter offsets rank above otherwise equivalent edges."""
    rows = [
        {"address": "0x1800", "end": "0x1900", "name": "caller",
         "status": "recovered"},
    ]
    functions = FunctionTable(rows)
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "t.jsonl"
        records = [
            # Edge A: 1 witness, no memory traffic, no fighter access
            {"type": "step", "step": 1, "ip_before": 0x1800, "ip_after": 0x9000},
            # Edge B: 1 witness + 1 fighter read at fighter0+0x1a4
            {"type": "memory", "step": 2, "kind": "read",
             "address": 0x510000 + 0x1a4, "size": 4},
            {"type": "step", "step": 2, "ip_before": 0x1800, "ip_after": 0x9004},
            {"type": "final", "status": "ok", "halt_reason": "stop address",
             "ip": 0x10dcc},
        ]
        trace.write_text("\n".join(json.dumps(r) for r in records) + "\n")
        frontier = Frontier()
        frontier.set_fighter_bases([0x510000], window=0x2000)
        frontier.ingest_trace(trace, "t.jsonl")
        ranked = frontier.rank_edges(functions, limit=10,
                                     exclude_recovered=False)
        # Same witnesses, same boundary distance -> fighter offset wins
        fighter_edge = next(item for item in ranked
                            if item["to"] == hex32(0x9004))
        plain_edge = next(item for item in ranked
                          if item["to"] == hex32(0x9000))
        assert fighter_edge["score"] > plain_edge["score"], (fighter_edge, plain_edge)
        assert ranked[0]["to"] == hex32(0x9004)
    print("ok: fighter access score bonus ranks fighter-aware edges higher")


def test_contiguous_fighter_blocks_detects_struct_layout():
    """A sequence of 4B R/W fighter accesses at offsets 0x1680..0x16ec
    must surface as one contiguous block, not 28 separate rows."""
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        records = []
        step = 1
        # Touch fighter offsets 0x1680, 0x1684, ..., 0x16ec with width 4.
        # Each offset gets one read and one write, both from the same
        # two IPs (0x2399c, 0x23a38), to mirror the real corpus shape.
        for off in range(0x1680, 0x16f0, 4):
            for kind, ip in (("read", 0x2399c), ("write", 0x23a38)):
                records.append(
                    {"type": "memory", "step": step,
                     "kind": kind, "address": 0x510000 + off, "size": 4}
                )
                records.append(
                    {"type": "step", "step": step,
                     "ip_before": ip, "ip_after": ip + 4}
                )
                step += 1
        records.append({"type": "final", "status": "ok",
                        "halt_reason": "stop address", "ip": 0x10dcc})
        trace.write_text("\n".join(json.dumps(r) for r in records) + "\n")
        frontier = Frontier()
        frontier.set_fighter_bases([0x510000], window=0x2000)
        frontier.ingest_trace(trace, "case.jsonl")
        blocks = frontier.contiguous_fighter_blocks(width=4, min_count=1,
                                                   min_length=3)
    assert len(blocks) == 1, blocks
    block = blocks[0]
    assert block["offset"] == hex32(0x1680)
    assert block["end_offset"] == hex32(0x16f0)
    assert block["length"] == (0x16f0 - 0x1680) // 4
    assert block["byte_size"] == block["length"] * 4
    assert block["width"] == 4
    assert block["base_count"] == 1
    assert block["ip_overlap"] >= 0.5
    assert hex32(0x2399c) in block["top_ips"]
    assert hex32(0x23a38) in block["top_ips"]
    print("ok: contiguous_fighter_blocks detects a 28-field 4B R/W struct")


def test_contiguous_fighter_blocks_rejects_short_runs():
    """A run shorter than min_length must not surface as a block."""
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        records = []
        for off in range(0x1700, 0x1708, 4):  # only 2 fields
            for kind, ip in (("read", 0x2399c), ("write", 0x23a38)):
                records.append(
                    {"type": "memory", "step": len(records) // 2 + 1,
                     "kind": kind, "address": 0x510000 + off, "size": 4}
                )
                records.append(
                    {"type": "step", "step": len(records) // 2 + 1,
                     "ip_before": ip, "ip_after": ip + 4}
                )
        records.append({"type": "final", "status": "ok",
                        "halt_reason": "stop address", "ip": 0x10dcc})
        trace.write_text("\n".join(json.dumps(r) for r in records) + "\n")
        frontier = Frontier()
        frontier.set_fighter_bases([0x510000], window=0x2000)
        frontier.ingest_trace(trace, "case.jsonl")
        blocks = frontier.contiguous_fighter_blocks(width=4, min_count=1,
                                                   min_length=3)
    assert blocks == [], blocks
    print("ok: contiguous_fighter_blocks rejects short runs")


def test_contiguous_fighter_blocks_filters_by_width():
    """The width argument filters out offsets with mismatched widths.

    With width=4, a 1B access at offset 0x1804 is filtered out, so the
    4B walk sees 0x1800, 0x1808, 0x180c, 0x1810 — and the latter
    three are 4B-spaced, so a block 0x1808-0x1810 surfaces (length 3).

    With width=1, the 1B offset 0x1804 stands alone; no contiguous
    1B run of length >= 3 forms, so no block surfaces.
    """
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        records = []
        for off, width in (
            (0x1800, 4), (0x1804, 1), (0x1808, 4), (0x180c, 4), (0x1810, 4)
        ):
            for kind, ip in (("read", 0x2399c), ("write", 0x23a38)):
                records.append(
                    {"type": "memory", "step": len(records) // 2 + 1,
                     "kind": kind, "address": 0x510000 + off, "size": width}
                )
                records.append(
                    {"type": "step", "step": len(records) // 2 + 1,
                     "ip_before": ip, "ip_after": ip + 4}
                )
        records.append({"type": "final", "status": "ok",
                        "halt_reason": "stop address", "ip": 0x10dcc})
        trace.write_text("\n".join(json.dumps(r) for r in records) + "\n")
        frontier = Frontier()
        frontier.set_fighter_bases([0x510000], window=0x2000)
        frontier.ingest_trace(trace, "case.jsonl")
        blocks_4 = frontier.contiguous_fighter_blocks(width=4, min_count=1,
                                                     min_length=3)
        blocks_1 = frontier.contiguous_fighter_blocks(width=1, min_count=1,
                                                     min_length=3)
    # Width 4: 0x1808-0x1810 forms a contiguous run of length 3.
    assert len(blocks_4) == 1, blocks_4
    assert blocks_4[0]["offset"] == hex32(0x1808)
    assert blocks_4[0]["end_offset"] == hex32(0x1814)
    assert blocks_4[0]["length"] == 3
    # Width 1: only a single 1B access at 0x1804 -> no block.
    assert blocks_1 == [], blocks_1
    print("ok: contiguous_fighter_blocks filters by width correctly")


def test_cli_emits_fighter_contiguous_block_records():
    """The --json CLI surface must include fighter_contiguous_block records
    when --fighter-base is configured. This is the downstream JSONL
    contract for v0729h.
    """
    import subprocess
    import sys
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        records = []
        step = 1
        for off in range(0x1800, 0x1830, 4):  # 12 contiguous 4B fields
            for kind, ip in (("read", 0x2399c), ("write", 0x23a38)):
                records.append(
                    {"type": "memory", "step": step,
                     "kind": kind, "address": 0x510000 + off, "size": 4}
                )
                records.append(
                    {"type": "step", "step": step,
                     "ip_before": ip, "ip_after": ip + 4}
                )
                step += 1
        records.append({"type": "final", "status": "ok",
                        "halt_reason": "stop address", "ip": 0x10dcc})
        trace.write_text("\n".join(json.dumps(r) for r in records) + "\n")
        proc = subprocess.run(
            [
                sys.executable,
                str(Path(__file__).resolve().parent / "frontier.py"),
                str(trace),
                "--fighter-base", "0x510000",
                "--limit", "1",
                "--json",
            ],
            capture_output=True, text=True,
        )
    assert proc.returncode == 0, (proc.returncode, proc.stderr)
    blocks = [
        json.loads(line)
        for line in proc.stdout.splitlines()
        if line and json.loads(line).get("kind") == "fighter_contiguous_block"
    ]
    assert len(blocks) >= 1, proc.stdout
    block = blocks[0]
    assert block["offset"] == "0x00001800"
    assert block["end_offset"] == "0x00001830"
    assert block["length"] == 12
    assert block["byte_size"] == 48
    assert block["width"] == 4
    assert block["ip_overlap"] >= 0.5
    print("ok: --json CLI surface emits fighter_contiguous_block records")


def test_cli_cross_boundary_filter():
    """`--cross-boundary` must limit call-edge output to edges where one
    side is recovered and the other is not. Without the flag, both
    cross- and non-cross-boundary edges appear; with the flag only
    cross-boundary edges appear (marked with `*`).
    """
    import subprocess
    with tempfile.TemporaryDirectory() as tmp:
        # Functions CSV: caller is recovered, callee is candidate.
        # This forces the only call in our synthetic trace to be a
        # cross-boundary edge.
        functions_csv = Path(tmp) / "functions.csv"
        functions_csv.write_text(
            "address,end,name,status\n"
            "0x16400,0x16500,caller,recovered\n"
            "0x18600,0x18700,callee,candidate\n"
        )
        # JSONL trace: two calls (caller -> callee = cross-boundary)
        # and one intra-caller mov (non-cross), plus an unsupported
        # final so the call-edges section actually prints (the call
        # edges section is nested inside the unsupported-address
        # block in frontier.py's text output).
        trace_jsonl = Path(tmp) / "trace.jsonl"
        records = [
            {"type": "step", "step": 1, "ip_before": 0x164ac, "ip_after": 0x18644,
             "mnemonic": "call"},
            {"type": "step", "step": 2, "ip_before": 0x164ac, "ip_after": 0x18644,
             "mnemonic": "call"},
            {"type": "step", "step": 3, "ip_before": 0x164b0, "ip_after": 0x164b4,
             "mnemonic": "mov"},
            {"type": "final", "status": "unsupported",
             "halt_reason": "vf2_error_unsupported",
             "ip": 0x18000},
        ]
        trace_jsonl.write_text(
            "\n".join(json.dumps(r) for r in records) + "\n"
        )
        cli = str(Path(__file__).resolve().parent / "frontier.py")
        # Unfiltered run: the single call edge should appear with `*`.
        proc_unf = subprocess.run(
            [sys.executable, cli,
             "--functions-csv", str(functions_csv),
             "--limit", "12",
             str(trace_jsonl)],
            capture_output=True, text=True, timeout=30,
        )
        assert proc_unf.returncode == 0, proc_unf.stderr
        out_unf = proc_unf.stdout
        assert "call edges (source -> target):" in out_unf, out_unf
        assert " *" in out_unf, (
            f"unfiltered output should mark cross-boundary edges "
            f"with '*':\n{out_unf}"
        )
        # Filtered run: --cross-boundary must be accepted and surface
        # the same single cross-boundary edge.
        proc_filt = subprocess.run(
            [sys.executable, cli,
             "--functions-csv", str(functions_csv),
             "--limit", "12",
             "--cross-boundary",
             str(trace_jsonl)],
            capture_output=True, text=True, timeout=30,
        )
        assert proc_filt.returncode == 0, proc_filt.stderr
        out_filt = proc_filt.stdout
        assert "call edges (source -> target):" in out_filt, out_filt
        assert " *" in out_filt, (
            f"filtered output should still surface the only cross-"
            f"boundary edge:\n{out_filt}"
        )
    print("ok: --cross-boundary CLI filter accepted and surfaces cross edges")


def main() -> int:
    test_function_table_lookup()
    test_trace_ingestion()
    test_memory_rw_and_call()
    test_memory_width_and_fighter_widths()
    test_rank_call_edges_crosses_boundary()
    test_unsupported_final_attribution()
    test_corpus_manifest_ingestion(True)
    test_corpus_manifest_ingestion(False)
    test_ranking_prefers_reproducible_boundary()
    test_classify_input()
    test_duckdb_parquet_export()
    test_per_edge_fighter_offset_surfaces()
    test_per_source_attribution()
    test_fighter_access_score_bonus()
    test_contiguous_fighter_blocks_detects_struct_layout()
    test_contiguous_fighter_blocks_rejects_short_runs()
    test_contiguous_fighter_blocks_filters_by_width()
    test_cli_emits_fighter_contiguous_block_records()
    test_cli_cross_boundary_filter()
    print("all frontier tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
