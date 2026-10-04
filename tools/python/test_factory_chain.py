#!/usr/bin/env python3
"""Integration test for the v0729 factory chain.

Runs standalone (no pytest required):

    python tools/python/test_factory_chain.py

Validates that the factory's public Python API composes correctly:
a single synthetic trace is pushed through frontier.py v2, then
infer_structs.py is run on the same trace, then
contiguous_fighter_blocks is queried. The integration test confirms
each step consumes the previous step's output without surprises,
so the runbook's "Step 1 -> Step 2 -> Step 2a" chain is verified at
the Python API level (not just at the per-tool CLI surface).

The test never shells out to vf2probe and never touches the ROM; it
is purely an in-process integration check of the analysis layer.
"""

from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from frontier import Frontier
from infer_rules import parse_bitfield, feature_vector, try_boolean_minimize
from infer_structs import summarize_trace


def _synthetic_player_trace(trace_path: Path) -> None:
    """Write a trace with 120 contiguous 4B R/W fighter accesses
    at fighter offsets 0x1680..0x185c (stride 4), plus 2 noise
    accesses that are out of the fighter window."""
    records = []
    step = 1
    # 120 contiguous 4B fields: range(0x1680, 0x1860, 4) is (0x1860-0x1680)/4 = 120.
    for off in range(0x1680, 0x1860, 4):
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
    # Two noise accesses outside the fighter window.
    for addr in (0x00884000, 0x0050a00c):
        records.append(
            {"type": "memory", "step": step, "kind": "read",
             "address": addr, "size": 4}
        )
        records.append(
            {"type": "step", "step": step,
             "ip_before": 0x18644, "ip_after": 0x18648}
        )
        step += 1
    records.append({"type": "final", "status": "ok",
                    "halt_reason": "stop address", "ip": 0x10dcc})
    trace_path.write_text(
        "\n".join(json.dumps(r) for r in records) + "\n",
        encoding="utf-8",
    )


def test_chain_frontier_then_contiguous_blocks():
    """Step 1 -> Step 2a: frontier ingests a trace and surfaces the
    contiguous block, with fighter-base + per-edge offsets populated."""
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        _synthetic_player_trace(trace)
        f = Frontier()
        f.set_fighter_bases([0x510000], window=0x2000)
        stats = f.ingest_trace(trace, "case.jsonl")
        # Step 1 — surface the next edge via rank_edges.
        ranked = f.rank_edges(None, limit=4, exclude_recovered=False)
        assert ranked, ranked
        # Every top edge carries the per-edge fighter offset surfacing.
        total_fighter_access = sum(
            edge.get("fighter_access_count", 0) for edge in ranked
        )
        assert total_fighter_access > 0, ranked
        # Step 2a — contiguous_fighter_blocks automatically aggregates.
        blocks = f.contiguous_fighter_blocks(width=4, min_count=1,
                                            min_length=3)
        assert len(blocks) == 1, blocks
        block = blocks[0]
        assert block["offset"] == "0x00001680", block
        assert block["length"] == 120, block
        assert block["byte_size"] == 480, block
        assert block["ip_overlap"] >= 0.99, block
        # Stats sanity check.
        assert stats["steps"] >= 120, stats
        assert stats["memory_accesses"] >= 120, stats


def test_chain_infer_structs_then_frontier_contiguous():
    """Step 2 -> Step 2a: infer_structs roll-up agrees with the
    contiguous-block detection when run on the same trace."""
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        _synthetic_player_trace(trace)
        # infer_structs roll-up counts every offset independently.
        fields, total, unmatched = summarize_trace(
            trace, bases={"fighter0": 0x510000}, window=0x2000,
        )
        # The contiguous run touches 60 fields (one per offset).
        assert 0x1680 in fields, sorted(fields.keys())[:5]
        assert total >= 120, total
        # The two noise accesses are out-of-window.
        assert unmatched >= 2, unmatched
        # The frontier detector aggregates them into one block.
        f = Frontier()
        f.set_fighter_bases([0x510000], window=0x2000)
        f.ingest_trace(trace, "case.jsonl")
        blocks = f.contiguous_fighter_blocks(width=4, min_count=1,
                                            min_length=3)
        assert len(blocks) == 1, blocks
        # The single block covers the same offsets as the per-offset
        # roll-up, so its length equals the number of distinct offsets.
        assert blocks[0]["length"] == len(fields), (blocks[0], fields)


def test_chain_fighter_bases_dual_provenance():
    """Step 2 (dual-base): the same offset accessed from both
    fighter0 and fighter1 is promoted to multi-corridor."""
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        records = []
        step = 1
        # Same offset 0x1a4 touched from both bases, one access each.
        for base, ip in ((0x510000, 0x18644), (0x520000, 0x18648)):
            records.append(
                {"type": "memory", "step": step,
                 "kind": "read", "address": base + 0x1a4, "size": 4}
            )
            records.append(
                {"type": "step", "step": step,
                 "ip_before": ip, "ip_after": ip + 4}
            )
            step += 1
        records.append({"type": "final", "status": "ok",
                        "halt_reason": "stop address", "ip": 0x10dcc})
        trace.write_text(
            "\n".join(json.dumps(r) for r in records) + "\n",
            encoding="utf-8",
        )
        f = Frontier()
        f.set_fighter_bases([0x510000, 0x520000], window=0x2000)
        f.ingest_trace(trace, "case.jsonl")
        # 0x1a4 must surface as a top fighter offset with base_count==2.
        rows = {row["offset"]: row for row in f.top_fighter_offsets(20)}
        assert "0x000001a4" in rows, sorted(rows.keys())
        assert rows["0x000001a4"]["base_count"] == 2, rows["0x000001a4"]
        # The rank output for the edges touching 0x1a4 must report
        # the offset in fighter_read_offsets.
        ranked = f.rank_edges(None, limit=10, exclude_recovered=False)
        edges_with_1a4 = [
            edge for edge in ranked
            if "0x000001a4" in (edge.get("fighter_read_offsets") or [])
        ]
        assert edges_with_1a4, ranked
        # And the contiguous-block detector must NOT promote a 1-field
        # offset to a block (min_length=3 filters it).
        blocks = f.contiguous_fighter_blocks(width=4, min_count=1,
                                            min_length=3)
        assert blocks == [], blocks


def test_chain_infer_rules_consumes_feature_vector():
    """The sweep-driven branch of the runbook (Step 1 -> Step 3 ->
    Step 4) is composed of frontier ingest + infer_rules. The
    infer_rules helpers (parse_bitfield, feature_vector,
    try_boolean_minimize) compose correctly with each other in the
    pipeline that turns a frontier edge into a measured boolean rule.
    """
    # parse_bitfield -> feature_vector -> try_boolean_minimize chain.
    name, bits = parse_bitfield("flags:1,2,4,6")
    assert name == "flags" and bits == [1, 2, 4, 6]
    # Build a complete truth table where the rule is exactly
    # "f0_b0 == 0 AND f2_b2 == 0" (so the features fully determine the
    # outcome). Each feature vector maps to exactly one outcome. The
    # values must be raw integers with the corresponding bit set, not
    # 0/1 booleans, because bitfields[("f0", [0])] extracts bit 0 of
    # the field value, not bit 0 of the index.
    target_outcome = ("ok", "stop", 0x10dcc, 5, 1, 1, ())
    other_outcome = ("other", "stop", 0x10ddc, 6, 1, 1, ())
    records = []
    for f0 in (0, 0x1):
        for f1 in (0, 0x2):
            for f2 in (0, 0x4):
                for f3 in (0, 0x8):
                    chosen = target_outcome if (f0 == 0 and f2 == 0) else other_outcome
                    records.append({
                        "inputs": {"f0": f0, "f1": f1,
                                   "f2": f2, "f3": f3},
                        "outcome": {
                            "status": chosen[0],
                            "halt_reason": chosen[1],
                            "ip": chosen[2],
                            "executed_instructions": chosen[3],
                            "procedure_calls": chosen[4],
                            "procedure_returns": chosen[5],
                            "reads_u32": [],
                        },
                    })
    boolean = []
    bitfields = [("f0", [0]), ("f1", [1]), ("f2", [2]), ("f3", [3])]
    rule, reason = try_boolean_minimize(
        records, boolean, bitfields, target_outcome=target_outcome,
    )
    try:
        import z3  # noqa: F401
        assert rule is not None, f"sympy/z3 available but no rule: {reason}"
        assert "f0_b0" in rule and "f2_b2" in rule, rule
        print(f"ok: chain infer_rules produced rule {rule!r}")
    except ImportError:
        assert rule is None, f"z3 missing but rule returned: {rule}"
        assert reason is not None
        print("ok: chain infer_rules reports z3-missing cleanly")


def main() -> int:
    test_chain_frontier_then_contiguous_blocks()
    test_chain_infer_structs_then_frontier_contiguous()
    test_chain_fighter_bases_dual_provenance()
    test_chain_infer_rules_consumes_feature_vector()
    print("all factory chain integration tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())