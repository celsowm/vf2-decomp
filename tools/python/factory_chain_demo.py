#!/usr/bin/env python3
"""Demonstrate the v0729 factory chain from the command line.

This script is a self-contained example of how the factory tooling
composes for a single recovery slice:

  Step 1 (frontier.py v2)  -> surface the next edge
  Step 2a (contiguous blocks) -> detect struct-like candidates
  Step 2 (infer_structs.py) -> corroborate per-offset roll-up
  Step 2 (dual-base) -> multi-corridor promotion
  Step 2c (rank_call_edges) -> surface cross-boundary call edges
  Step 3 (taint.py)  -> characterise the branch dependency
                          (skipped here; requires ROM + ROM-backed
                          vf2probe runs; the runbook documents this
                          step)
  Step 4 (ctest) -> gate every promotion with the factory test gate.

Run from the repository root:

    python tools/python/factory_chain_demo.py

Exits 0 on success, non-zero on the first failing step. The script
deliberately uses synthetic traces so no ROM is required.
"""

from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from frontier import Frontier
from infer_structs import summarize_trace


def _write_trace(path: Path, base: int, base2: int) -> None:
    """120 contiguous 4B R/W fighter accesses at offsets
    0x1680..0x185c from fighter0, plus 0x1a4 touched from both
    fighter0 AND fighter1 (dual-base trigger), plus a same-function
    call and a cross-boundary call for Step 2c.
    """
    records = []
    step = 1
    for off in range(0x1680, 0x1860, 4):
        for kind, ip in (("read", 0x2399c), ("write", 0x23a38)):
            records.append(
                {"type": "memory", "step": step,
                 "kind": kind, "address": base + off, "size": 4}
            )
            records.append(
                {"type": "step", "step": step,
                 "ip_before": ip, "ip_after": ip + 4}
            )
            step += 1
    # Dual-base trigger: same offset 0x1a4 from fighter0 AND fighter1.
    for trigger_base in (base, base2):
        records.append(
            {"type": "memory", "step": step,
             "kind": "read", "address": trigger_base + 0x1a4, "size": 4}
        )
        records.append(
            {"type": "step", "step": step,
             "ip_before": 0x18644, "ip_after": 0x18648}
        )
        step += 1
    # Step 2c: same-function call (caller -> caller = non-cross) and
    # cross-boundary call (caller -> callee = cross).
    records.append(
        {"type": "step", "step": step,
         "ip_before": 0x164c0, "ip_after": 0x16480,
         "mnemonic": "call"}
    )
    step += 1
    records.append(
        {"type": "step", "step": step,
         "ip_before": 0x164ac, "ip_after": 0x18644,
         "mnemonic": "call"}
    )
    step += 1
    records.append({"type": "final", "status": "ok",
                    "halt_reason": "stop address", "ip": 0x10dcc})
    path.write_text(
        "\n".join(json.dumps(r) for r in records) + "\n",
        encoding="utf-8",
    )


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "demo.jsonl"
        base = 0x510000
        base2 = base + 0x10000  # fighter1 base offset for dual-base trigger
        _write_trace(trace, base, base2)

        # --- Step 1: ingest + rank -----------------------------------
        f = Frontier()
        f.set_fighter_bases([base, base2], window=0x2000)
        f.ingest_trace(trace, "demo.jsonl")
        ranked = f.rank_edges(None, limit=4, exclude_recovered=False)
        if not ranked:
            print("FAIL: Step 1 produced no edges", file=sys.stderr)
            return 1
        print(f"Step 1: surfaced {len(ranked)} ranked edges")

        # --- Step 2a: contiguous blocks ------------------------------
        blocks = f.contiguous_fighter_blocks(
            width=4, min_count=1, min_length=3,
        )
        if not blocks:
            print("FAIL: Step 2a found no contiguous blocks", file=sys.stderr)
            return 1
        main_block = max(blocks, key=lambda b: b["length"])
        print(
            f"Step 2a: contiguous block {main_block['offset']}.."
            f"{main_block['end_offset']} length={main_block['length']} "
            f"size={main_block['byte_size']}B "
            f"ip_overlap={main_block['ip_overlap']}"
        )

        # --- Step 2: infer_structs per-offset roll-up ----------------
        fields, total, unmatched = summarize_trace(
            trace, bases={"fighter0": base}, window=0x2000,
        )
        # infer_structs sees every distinct offset, including 0x1a4
        # which is NOT contiguous with the 0x1680 block (it sits ~0.15KB
        # earlier). So the per-offset count is main_block.length + 1.
        expected_offset_count = main_block["length"] + 1
        if len(fields) != expected_offset_count:
            print(
                f"FAIL: per-offset count {len(fields)} != "
                f"block_length {main_block['length']} + 1 "
                f"({expected_offset_count})", file=sys.stderr,
            )
            return 1
        print(
            f"Step 2: infer_structs roll-up shows "
            f"{len(fields)} distinct offsets ({main_block['length']} "
            f"in the contiguous block + 1 standalone at 0x1a4)"
        )

        # --- Step 2 (dual-base): 0x1a4 from both bases ----------------
        rows = {row["offset"]: row for row in f.top_fighter_offsets(40)}
        if "0x000001a4" not in rows or rows["0x000001a4"]["base_count"] != 2:
            print(
                f"FAIL: 0x1a4 dual-base missing or wrong: {rows.get('0x000001a4')}",
                file=sys.stderr,
            )
            return 1
        print(
            f"Step 2 (dual-base): fighter0 + fighter1 at 0x1a4 promoted "
            f"to base_count={rows['0x000001a4']['base_count']}"
        )

        # --- Step 2c: rank_call_edges with crosses_boundary ----------
        # Build a synthetic FunctionTable so crosses_boundary is
        # meaningful: 0x16400..0x16500 = "caller" (recovered),
        # 0x18600..0x18700 = "callee" (candidate). The 0x164ac ->
        # 0x18644 call crosses the boundary; the 0x164c0 -> 0x16480
        # call is same-function (non-cross).
        from frontier import FunctionTable
        functions = FunctionTable([
            {"address": "0x16400", "end": "0x16500",
             "name": "caller", "status": "recovered"},
            {"address": "0x18600", "end": "0x18700",
             "name": "callee", "status": "candidate"},
        ])
        call_edges = f.rank_call_edges(functions, limit=10)
        if not call_edges:
            print("FAIL: Step 2c found no call edges", file=sys.stderr)
            return 1
        cross_edges = [e for e in call_edges if e["crosses_boundary"]]
        if not cross_edges:
            print(
                "FAIL: Step 2c found no cross-boundary call edges; "
                f"got {len(call_edges)} total edges",
                file=sys.stderr,
            )
            return 1
        if not any(
            e["from"] == "0x000164ac" and e["to"] == "0x00018644"
            for e in cross_edges
        ):
            print(
                f"FAIL: Step 2c cross-boundary edges missing the "
                f"0x164ac->0x18644 edge: {cross_edges}",
                file=sys.stderr,
            )
            return 1
        # Non-cross same-function call must also surface unfiltered.
        if not any(
            e["from"] == "0x000164c0" and e["to"] == "0x00016480"
            and not e["crosses_boundary"]
            for e in call_edges
        ):
            print(
                f"FAIL: Step 2c non-cross same-function call missing: "
                f"{call_edges}",
                file=sys.stderr,
            )
            return 1
        # Filtering by crosses_boundary must drop the same-function edge.
        filtered = [e for e in call_edges if e["crosses_boundary"]]
        if any(e["from"] == "0x000164c0" for e in filtered):
            print(
                "FAIL: Step 2c cross-boundary filter leaked the "
                "same-function edge",
                file=sys.stderr,
            )
            return 1
        print(
            f"Step 2c: rank_call_edges surfaces {len(call_edges)} call "
            f"edges, {len(cross_edges)} cross-boundary "
            f"(0x164ac->0x18644 marked)"
        )

        print()
        print("PASS: factory chain composed end-to-end on synthetic trace")
        print(f"      (see Step 3 taint docs in factory_runbook_v0729.md "
              f"for the ROM-backed path)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())