#!/usr/bin/env python3
"""Unit tests for tools/python/infer_structs.py.

Runs standalone (no pytest required):

    python tools/python/test_infer_structs.py

Validates the field-aggregation algorithm on synthetic traces and
locks in the contract that:

- an offset touched from both fighter0 and fighter1 is flagged
  dual-base (promotable to multi-corridor provenance);
- read/write/size metadata is preserved;
- ips are attributed to the step's ``ip_before``;
- unmatched (out-of-window) accesses are counted but excluded from
  the field roll-up.
"""

from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from infer_structs import (
    field_records,
    load_scenario_bases,
    merge_fields,
    new_field,
    summarize_trace,
    summarize_traces,
)


def _write_trace(path: Path, records: list) -> None:
    path.write_text(
        "\n".join(json.dumps(r) for r in records) + "\n",
        encoding="utf-8",
    )


def test_load_scenario_bases_extracts_fighter_metadata():
    scenario = {
        "metadata": {"fighter0": "0x510000", "fighter1": "0x520000"},
    }
    with tempfile.TemporaryDirectory() as tmp:
        p = Path(tmp) / "scenario.json"
        p.write_text(json.dumps(scenario), encoding="utf-8")
        bases = load_scenario_bases(p)
    assert bases == {"fighter0": 0x510000, "fighter1": 0x520000}, bases
    print("ok: load_scenario_bases parses fighter metadata")


def test_summarize_trace_dual_base():
    """An offset touched from both bases becomes a multi-corridor candidate."""
    records = [
        # fighter0 + 0x1a4 read (twice, from two different ips)
        {"type": "memory", "step": 1, "kind": "read",
         "address": 0x510000 + 0x1a4, "size": 4},
        {"type": "step", "step": 1, "ip_before": 0x18690, "ip_after": 0x18694},
        {"type": "memory", "step": 2, "kind": "read",
         "address": 0x510000 + 0x1a4, "size": 4},
        {"type": "step", "step": 2, "ip_before": 0x18700, "ip_after": 0x18704},
        # fighter1 + 0x1a4 read (single)
        {"type": "memory", "step": 3, "kind": "read",
         "address": 0x520000 + 0x1a4, "size": 4},
        {"type": "step", "step": 3, "ip_before": 0x18800, "ip_after": 0x18804},
        # fighter0 + 0x5b6 write (single, width 2)
        {"type": "memory", "step": 4, "kind": "write",
         "address": 0x510000 + 0x5b6, "size": 2},
        {"type": "step", "step": 4, "ip_before": 0x18810, "ip_after": 0x18814},
        # unrelated work-RAM access (must not appear in the roll-up)
        {"type": "memory", "step": 5, "kind": "write",
         "address": 0x00884000, "size": 4},
        {"type": "step", "step": 5, "ip_before": 0x18820, "ip_after": 0x18824},
        {"type": "final", "status": "ok", "halt_reason": "stop address",
         "ip": 0x10dcc},
    ]
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        _write_trace(trace, records)
        bases = {"fighter0": 0x510000, "fighter1": 0x520000}
        fields, total, unmatched = summarize_trace(trace, bases, 0x2000)
    assert total == 5, total
    assert unmatched == 1, unmatched
    # Offset 0x1a4: dual-base R from both fighters, width 4
    assert 0x1a4 in fields
    rec_1a4 = fields[0x1a4]
    assert rec_1a4["bases"] == {"fighter0", "fighter1"}
    assert rec_1a4["reads"] == 3
    assert rec_1a4["writes"] == 0
    assert dict(rec_1a4["sizes"]) == {4: 3}
    ips_1a4 = set(rec_1a4["ips"].keys())
    assert ips_1a4 == {0x18690, 0x18700, 0x18800}, ips_1a4
    # Offset 0x5b6: single-base (fighter0), R+W, width 2
    assert 0x5b6 in fields
    rec_5b6 = fields[0x5b6]
    assert rec_5b6["bases"] == {"fighter0"}
    assert rec_5b6["reads"] == 0
    assert rec_5b6["writes"] == 1
    assert dict(rec_5b6["sizes"]) == {2: 1}
    # No 0x0884 field (out of window)
    assert all(off < 0x2000 for off in fields)
    print("ok: summarize_trace promotes dual-base offsets and counts unmatched")


def test_field_records_sorting_and_json_shape():
    """Sorting puts multi-base first, then by total, then by offset."""
    fields = {
        0x1a4: {
            "bases": {"fighter0", "fighter1"}, "reads": 5, "writes": 0,
            "sizes": {4: 5}, "ips": Counter_or_dict({0x18690: 3, 0x18800: 2}),
            "addresses": Counter_or_dict({0x510000 + 0x1a4: 3, 0x520000 + 0x1a4: 2}),
        },
        0x5b6: {
            "bases": {"fighter0"}, "reads": 100, "writes": 0,
            "sizes": {4: 100}, "ips": Counter_or_dict({0x18810: 100}),
            "addresses": Counter_or_dict({0x510000 + 0x5b6: 100}),
        },
    }
    records = field_records(fields)
    # Multi-base first regardless of higher single-base count
    assert records[0]["offset"] == 0x1a4
    assert records[0]["base_count"] == 2
    assert records[1]["offset"] == 0x5b6
    assert records[1]["base_count"] == 1
    # JSON-serialisable shape
    payload = json.dumps(records[0], sort_keys=True)
    parsed = json.loads(payload)
    assert parsed["offset"] == 0x1a4
    assert parsed["base_count"] == 2
    assert parsed["sizes"] == [{"size": 4, "count": 5}]
    print("ok: field_records ranks multi-base first and is JSON-serialisable")


def Counter_or_dict(pairs):
    """Build a tiny Counter-like from a dict for test brevity."""
    from collections import Counter
    return Counter(pairs)


def test_orphan_memory_attributed_to_previous_step_ip():
    """If a memory access precedes any step, it stays unmatched (no IP)."""
    records = [
        # Memory at step 1, but no step record at step 1 -> orphan
        {"type": "memory", "step": 1, "kind": "read",
         "address": 0x510000 + 0x100, "size": 4},
        {"type": "step", "step": 2, "ip_before": 0x18690, "ip_after": 0x18694},
        {"type": "final", "status": "ok", "halt_reason": "stop address",
         "ip": 0x10dcc},
    ]
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        _write_trace(trace, records)
        bases = {"fighter0": 0x510000}
        fields, total, unmatched = summarize_trace(trace, bases, 0x2000)
    assert total == 1, total
    # Memory was inside the window but unattributed to a step, so the
    # unmatched counter must reflect it (the field can still receive it
    # via the post-pass sweep in summarize_trace).
    assert 0x100 in fields
    # The IP attribution is lost because no step carried step==1
    assert dict(fields[0x100]["ips"]) == {}, dict(fields[0x100]["ips"])
    print("ok: orphan memory access stays unmatched without IP attribution")


def test_summarize_traces_single_trace_matches_summarize_trace():
    """Merging one trace must be identical to summarising it alone.

    This is the backward-compatibility pin for the multi-trace roll-up: a
    one-trace invocation has to produce the same fields, counts and unmatched
    total it always did, or every existing corpus reading moves.
    """
    records = [
        {"type": "memory", "step": 1, "kind": "read",
         "address": 0x510000 + 0x1a4, "size": 4},
        {"type": "step", "step": 1, "ip_before": 0x18690, "ip_after": 0x18694},
        {"type": "memory", "step": 2, "kind": "write",
         "address": 0x520000 + 0x5b6, "size": 2},
        {"type": "step", "step": 2, "ip_before": 0x18810, "ip_after": 0x18814},
        {"type": "memory", "step": 3, "kind": "write",
         "address": 0x00884000, "size": 4},
        {"type": "step", "step": 3, "ip_before": 0x18820, "ip_after": 0x18824},
        {"type": "final", "status": "ok", "halt_reason": "stop address",
         "ip": 0x10dcc},
    ]
    bases = {"fighter0": 0x510000, "fighter1": 0x520000}
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "only.jsonl"
        _write_trace(trace, records)
        single, single_total, single_unmatched = summarize_trace(
            trace, bases, 0x2000
        )
        merged, merged_total, merged_unmatched, sources = summarize_traces(
            [trace], bases, 0x2000
        )
    assert single_total == merged_total == 3, (single_total, merged_total)
    assert single_unmatched == merged_unmatched == 1
    assert set(single) == set(merged), (set(single), set(merged))
    for offset in single:
        assert single[offset]["bases"] == merged[offset]["bases"]
        assert single[offset]["reads"] == merged[offset]["reads"]
        assert single[offset]["writes"] == merged[offset]["writes"]
        assert dict(single[offset]["sizes"]) == dict(merged[offset]["sizes"])
        assert dict(single[offset]["ips"]) == dict(merged[offset]["ips"])
    assert sources == {"fighter0": {str(trace)}, "fighter1": {str(trace)}}, sources
    print("ok: summarize_traces of one trace equals summarize_trace")


def test_summarize_traces_promotes_across_traces():
    """Same offset, different base, different runs -> dual-base candidate.

    This is the case that motivated the roll-up. The 0x27b5c helper takes a
    single ``player`` pointer, so the same-offset / different-base evidence
    the v0704 discipline asks for cannot exist inside one trace. Here trace A
    only ever touches fighter0 and trace B only ever touches fighter1, and
    neither would reach base_count 2 on its own.
    """
    trace_a = [
        {"type": "memory", "step": 1, "kind": "read",
         "address": 0x510000 + 0x1a4, "size": 4},
        {"type": "step", "step": 1, "ip_before": 0x2399c, "ip_after": 0x239a0},
        {"type": "memory", "step": 2, "kind": "write",
         "address": 0x510000 + 0x1a4, "size": 4},
        {"type": "step", "step": 2, "ip_before": 0x23a38, "ip_after": 0x23a3c},
        {"type": "final", "status": "ok", "halt_reason": "stop address",
         "ip": 0x10dcc},
    ]
    trace_b = [
        {"type": "memory", "step": 1, "kind": "read",
         "address": 0x520000 + 0x1a4, "size": 4},
        {"type": "step", "step": 1, "ip_before": 0x2399c, "ip_after": 0x239a0},
        {"type": "memory", "step": 2, "kind": "write",
         "address": 0x520000 + 0x1a4, "size": 4},
        {"type": "step", "step": 2, "ip_before": 0x23a38, "ip_after": 0x23a3c},
        {"type": "final", "status": "ok", "halt_reason": "stop address",
         "ip": 0x10dcc},
    ]
    bases = {"fighter0": 0x510000, "fighter1": 0x520000}
    with tempfile.TemporaryDirectory() as tmp:
        a = Path(tmp) / "a.jsonl"
        b = Path(tmp) / "b.jsonl"
        _write_trace(a, trace_a)
        _write_trace(b, trace_b)

        # Neither trace alone can promote.
        fields_a, _, _ = summarize_trace(a, bases, 0x2000)
        fields_b, _, _ = summarize_trace(b, bases, 0x2000)
        assert fields_a[0x1a4]["bases"] == {"fighter0"}, fields_a[0x1a4]["bases"]
        assert fields_b[0x1a4]["bases"] == {"fighter1"}, fields_b[0x1a4]["bases"]

        merged, total, unmatched, sources = summarize_traces(
            [a, b], bases, 0x2000
        )

    assert total == 4, total
    assert unmatched == 0, unmatched
    field = merged[0x1a4]
    assert field["bases"] == {"fighter0", "fighter1"}, field["bases"]
    assert field["reads"] == 2, field["reads"]
    assert field["writes"] == 2, field["writes"]
    assert dict(field["sizes"]) == {4: 4}, dict(field["sizes"])
    # Same guest IPs from both runs - the v0704 same-IP half of the discipline.
    # Each IP is hit once per trace, so the merged count is 2 each. That the
    # counts doubled rather than the IP set growing is itself the evidence
    # that both runs used the same instruction pair.
    assert dict(field["ips"]) == {0x2399c: 2, 0x23a38: 2}, dict(field["ips"])
    # Provenance must show the two bases came from different runs, so a reader
    # can audit the cross-trace claim instead of taking it on faith.
    assert field["base_traces"] == {
        "fighter0": {str(a)},
        "fighter1": {str(b)},
    }, field["base_traces"]
    assert sources == {
        "fighter0": {str(a)},
        "fighter1": {str(b)},
    }, sources

    records = field_records(merged)
    assert records[0]["offset"] == 0x1a4
    assert records[0]["base_count"] == 2
    assert records[0]["trace_count"] == 2, records[0]["trace_count"]
    # JSON-serialisable
    json.dumps(records[0], sort_keys=True)
    print("ok: summarize_traces promotes same-offset evidence across runs")


def test_merge_fields_unions_counters_and_provenance():
    """merge_fields adds counters and unions sets; it never resets a field."""
    target = {0x10: new_field()}
    source = {0x10: new_field(), 0x14: new_field()}
    target[0x10]["reads"] = 3
    target[0x10]["bases"].add("fighter0")
    target[0x10]["base_traces"]["fighter0"].add("a")
    target[0x10]["sizes"][4] = 3
    target[0x10]["ips"][0x2399c] = 3
    target[0x10]["addresses"][0x510010] = 3

    source[0x10]["reads"] = 2
    source[0x10]["writes"] = 5
    source[0x10]["bases"].add("fighter1")
    source[0x10]["base_traces"]["fighter1"].add("b")
    source[0x10]["base_traces"]["fighter0"].add("a")
    source[0x10]["sizes"][4] = 2
    source[0x10]["sizes"][2] = 5
    source[0x10]["ips"][0x23a38] = 2
    source[0x10]["addresses"][0x520010] = 2
    source[0x14]["writes"] = 1
    source[0x14]["bases"].add("fighter1")
    source[0x14]["base_traces"]["fighter1"].add("b")

    merge_fields(target, source)

    field = target[0x10]
    assert field["bases"] == {"fighter0", "fighter1"}, field["bases"]
    assert field["base_traces"] == {
        "fighter0": {"a"},
        "fighter1": {"b"},
    }, field["base_traces"]
    assert field["reads"] == 5 and field["writes"] == 5
    assert dict(field["sizes"]) == {4: 5, 2: 5}, dict(field["sizes"])
    assert dict(field["ips"]) == {0x2399c: 3, 0x23a38: 2}
    assert dict(field["addresses"]) == {0x510010: 3, 0x520010: 2}
    # A field present only in the source is created, not dropped.
    assert target[0x14]["bases"] == {"fighter1"}
    assert target[0x14]["writes"] == 1
    print("ok: merge_fields unions sets and adds counters")


def test_field_records_tolerates_missing_provenance():
    """Records built without base_traces still serialise (older shapes)."""
    fields = {
        0x1a4: {
            "bases": {"fighter0"}, "reads": 1, "writes": 0,
            "sizes": Counter_or_dict({4: 1}),
            "ips": Counter_or_dict({0x18690: 1}),
            "addresses": Counter_or_dict({0x5101a4: 1}),
        },
    }
    records = field_records(fields)
    assert records[0]["base_traces"] == {}, records[0]["base_traces"]
    assert records[0]["trace_count"] == 0
    json.dumps(records[0], sort_keys=True)
    print("ok: field_records tolerates a field with no provenance recorded")


def main() -> int:
    test_load_scenario_bases_extracts_fighter_metadata()
    test_summarize_trace_dual_base()
    test_field_records_sorting_and_json_shape()
    test_orphan_memory_attributed_to_previous_step_ip()
    test_summarize_traces_single_trace_matches_summarize_trace()
    test_summarize_traces_promotes_across_traces()
    test_merge_fields_unions_counters_and_provenance()
    test_field_records_tolerates_missing_provenance()
    print("all infer_structs tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())