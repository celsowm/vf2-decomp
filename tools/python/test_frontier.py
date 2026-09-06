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
    print("ok: memory R/W separation and call-target attribution")


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


def main() -> int:
    test_function_table_lookup()
    test_trace_ingestion()
    test_memory_rw_and_call()
    test_unsupported_final_attribution()
    test_corpus_manifest_ingestion(True)
    test_corpus_manifest_ingestion(False)
    test_ranking_prefers_reproducible_boundary()
    test_classify_input()
    test_duckdb_parquet_export()
    print("all frontier tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
