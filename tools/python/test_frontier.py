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
    print("ok: trace ingestion with step-correlated memory")


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


def test_call_target_attribution():
    rows = [
        {"address": "0x1000", "end": "0x1050", "name": "caller_fn", "status": "recovered"},
        {"address": "0x2000", "end": "0x2100", "name": "callee_fn", "status": "candidate"},
    ]
    functions = FunctionTable(rows)
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "t.jsonl"
        records = [
            {"type": "step", "step": 1, "ip_before": 0x1020, "ip_after": 0x2000, "is_call": True},
            {"type": "step", "step": 2, "ip_before": 0x1030, "ip_after": 0x1040},
        ]
        trace.write_text("\n".join(json.dumps(r) for r in records) + "\n")
        frontier = Frontier()
        frontier.ingest_trace(trace, "t.jsonl")
        ranked = frontier.rank_edges(functions, limit=10, exclude_recovered=False)
        call_edge = next(r for r in ranked if r["from"] == hex32(0x1020))
        jump_edge = next(r for r in ranked if r["from"] == hex32(0x1030))

        assert call_edge["is_call"] is True
        assert call_edge["call_target_name"] == "callee_fn"
        assert call_edge["call_target_status"] == "candidate"

        assert jump_edge["is_call"] is False
        assert jump_edge["call_target_name"] is None
    print("ok: call target attribution")


def test_classify_input():
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "t.jsonl"
        trace.write_text(json.dumps({"type": "step", "step": 1,
                                     "ip_before": 16, "ip_after": 20}) + "\n")
        assert classify_input(trace) == "trace"
        corpus = Path(tmp) / "m.jsonl"
        corpus.write_text(json.dumps({"case": 0, "inputs": {}, "new_edges": []}) + "\n")
        assert classify_input(corpus) == "corpus"
    print("ok: input classification")


def main() -> int:
    test_function_table_lookup()
    test_trace_ingestion()
    test_unsupported_final_attribution()
    test_corpus_manifest_ingestion(True)
    test_corpus_manifest_ingestion(False)
    test_ranking_prefers_reproducible_boundary()
    test_call_target_attribution()
    test_classify_input()
    print("all frontier tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
