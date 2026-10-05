#!/usr/bin/env python3
"""Real-corpus variant of factory_chain_demo for the P1 player slice.

This script exercises the v0729 factory tooling on the actual
``out/trace-both.jsonl`` and ``out/trace-f0.jsonl`` corpus artifacts
that back the P1 ``0x1680`` contiguous-fighter-block evidence
(see ``decomp/i960/notes/p1_player_0x1680_block_stability_v0730.md``
and ``decomp/i960/notes/fa_player_struct_1680_corpus_v0729.md``).

Composition (mirrors ``factory_chain_demo.py`` but on real traces):

  Step 1 (frontier.py v2)     -> ingest both traces, rank edges
  Step 2a (contiguous blocks) -> surface the 0xd00..0xee0 block
  Step 2 (infer_structs.py)  -> per-offset roll-up
  Step 2 (dual-base)         -> confirm base_count == 2 for the block
  Step 3 (taint.py)          -> skipped; taint refuses to print its
                          dependency report for the 0x2399c IP because
                          it is a store-quad instruction, not a
                          branch IP (see
                          ``decomp/i960/notes/p1_taint_0x1680_block_v0730.md``)

Run from the repository root:

    python tools/python/p1_real_trace_demo.py

Exits 0 on success, non-zero on the first failing step. Skips with
a clear message when the corpus traces are missing (the traces are
gitignored ROM-backed artefacts; the script does NOT regenerate
them).

The invariants this script locks in are the same ones the v0729
player-corpus smoke + 0x1680 notes recorded. Re-running this script
on a future corpus MUST reproduce the same block record
(length=120, byte_size=480, ip_overlap=1.0, base_count=2,
top_ips=[0x2399c, 0x23a38]) or the script fails.

**Fighter bases (corrected v0732k).** These are the MEASURED bases from
the ``fa_game_info`` scenario metadata (``out/state8-positive.json``):

    fighter0 = 0x00510980      fighter0_flags = 0x00510b24  (offset 0x1a4)
    fighter1 = 0x00512980      fighter1_flags = 0x00512b24  (offset 0x1a4)

The v0729/v0730 notes used 0x510000 / 0x520000 and reported the block at
offset 0x1680. That is the *same* 120 offsets displaced by exactly 0x980,
the difference between the wrong and the right fighter0 base - and with
the wrong bases the fighter1 window (0x520000..0x522000) does not contain
the real fighter1 struct at all, so every candidate silently reported
base_count == 1. See ``decomp/i960/notes/p1_fighter_bases_retraction_v0732k.md``.
"""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))

from frontier import Frontier  # noqa: E402
from infer_structs import summarize_trace  # noqa: E402

CORPUS = [
    ROOT / "out" / "trace-both.jsonl",
    ROOT / "out" / "trace-f0.jsonl",
]

# Invariants locked by the v0729 player-corpus smoke + v0730 P1 note.
# If the corpus changes, the detector parameters may need re-tuning
# (see p1_player_0x1680_block_stability_v0730.md).
#
# The block offset is expressed from the MEASURED fighter0 base 0x510980, so
# it reads 0x0d00 where the old 0x510000-based run read 0x1680. Same 120
# offsets, same 480 bytes, same guest IPs, same 240 reads + 240 writes per
# base - only the origin moved. See p1_fighter_bases_retraction_v0732k.md.
EXPECTED_OFFSET = "0x00000d00"
EXPECTED_END = "0x00000ee0"
EXPECTED_LENGTH = 120
EXPECTED_BYTE_SIZE = 480
EXPECTED_TOP_IPS = ["0x0002399c", "0x00023a38"]
EXPECTED_IP_OVERLAP = 1.0
# The block IS dual-base. It was measured as base_count == 1 only because the
# fighter1 window sat at 0x520000, which does not contain the real fighter1
# struct at 0x512980.
EXPECTED_BLOCK_BASE_COUNT = 2
EXPECTED_READS = 480
EXPECTED_WRITES = 480

# MEASURED bases, from the fa_game_info scenario metadata.
FIGHTER_BASES = [0x510980, 0x512980]
FIGHTER_WINDOW = 0x2000


def _require_corpus() -> bool:
    missing = [str(p) for p in CORPUS if not p.exists()]
    if missing:
        print(
            "skip: p1_real_trace_demo requires the following "
            "ROM-backed corpus artifacts:\n  "
            + "\n  ".join(missing)
            + "\nThese are gitignored; reproduce via the v0729 evidence "
            "pipeline (factory_runbook_v0729.md) or skip this entry."
        )
        return False
    return True


def main() -> int:
    if not _require_corpus():
        return 0  # skip rather than fail when corpus is absent

    # --- Step 1: ingest + rank -----------------------------------
    f = Frontier()
    f.set_fighter_bases(FIGHTER_BASES, window=FIGHTER_WINDOW)
    total_accesses = 0
    for trace in CORPUS:
        stats = f.ingest_trace(trace, trace.name)
        accesses = stats.get("memory_accesses", 0)
        total_accesses += accesses
        print(f"Step 1: ingested {trace.name}: {accesses} accesses")
    print(f"Step 1: total {total_accesses} accesses across both traces")

    # --- Step 2a: contiguous blocks ------------------------------
    blocks = f.contiguous_fighter_blocks(
        width=4,
        min_count=2,
        min_length=EXPECTED_LENGTH,
        ip_overlap=EXPECTED_IP_OVERLAP,
    )
    if not blocks:
        print(
            "FAIL: Step 2a found no contiguous 4B blocks at "
            f"length>={EXPECTED_LENGTH}, ip_overlap==1.0",
            file=sys.stderr,
        )
        return 1
    main_block = max(blocks, key=lambda b: b["length"])
    if main_block["offset"] != EXPECTED_OFFSET:
        print(
            f"FAIL: top block offset {main_block['offset']!r} != "
            f"expected {EXPECTED_OFFSET!r}",
            file=sys.stderr,
        )
        return 1
    if main_block["end_offset"] != EXPECTED_END:
        print(
            f"FAIL: top block end_offset {main_block['end_offset']!r} "
            f"!= expected {EXPECTED_END!r}",
            file=sys.stderr,
        )
        return 1
    if main_block["length"] != EXPECTED_LENGTH:
        print(
            f"FAIL: top block length {main_block['length']} != "
            f"expected {EXPECTED_LENGTH}",
            file=sys.stderr,
        )
        return 1
    if main_block["byte_size"] != EXPECTED_BYTE_SIZE:
        print(
            f"FAIL: top block byte_size {main_block['byte_size']} != "
            f"expected {EXPECTED_BYTE_SIZE}",
            file=sys.stderr,
        )
        return 1
    if main_block["ip_overlap"] != EXPECTED_IP_OVERLAP:
        print(
            f"FAIL: top block ip_overlap {main_block['ip_overlap']} "
            f"!= expected {EXPECTED_IP_OVERLAP}",
            file=sys.stderr,
        )
        return 1
    if main_block["top_ips"] != EXPECTED_TOP_IPS:
        print(
            f"FAIL: top block top_ips {main_block['top_ips']} != "
            f"expected {EXPECTED_TOP_IPS}",
            file=sys.stderr,
        )
        return 1
    if main_block["base_count"] != EXPECTED_BLOCK_BASE_COUNT:
        print(
            f"FAIL: top block base_count {main_block['base_count']} != "
            f"expected {EXPECTED_BLOCK_BASE_COUNT}. A drop here means the "
            "fighter bases are wrong: the measured pair is 0x510980 / "
            "0x512980, and 0x510000 / 0x520000 silently reports 1 because the "
            "fake fighter1 window misses the real struct.",
            file=sys.stderr,
        )
        return 1
    # Pin the per-base read/write balance too. 240 each per base is the
    # v0729 figure, and it is what distinguishes "both bases touch the block"
    # from "one base touches it twice as often".
    for label, key, expected in (("reads", "reads", EXPECTED_READS),
                                 ("writes", "writes", EXPECTED_WRITES)):
        actual = main_block.get(key)
        if actual != expected:
            print(
                f"FAIL: top block {label} {actual} != expected {expected}",
                file=sys.stderr,
            )
            return 1
    print(
        f"Step 2a: contiguous block {main_block['offset']}.."
        f"{main_block['end_offset']} length={main_block['length']} "
        f"size={main_block['byte_size']}B "
        f"ip_overlap={main_block['ip_overlap']} "
        f"top_ips={main_block['top_ips']} "
        f"base_count={main_block['base_count']}"
    )

    # --- Step 2: infer_structs per-offset roll-up ----------------
    # Both bases this time, so the roll-up itself has to report the
    # shared offsets rather than only the fighter0 half.
    fields, total, unmatched = summarize_trace(
        CORPUS[0],
        bases={
            "fighter0": FIGHTER_BASES[0],
            "fighter1": FIGHTER_BASES[1],
        },
        window=FIGHTER_WINDOW,
    )
    # The block contributes EXPECTED_LENGTH distinct offsets (120 in the
    # contiguous block). The exact total depends on what other offsets
    # `trace-both.jsonl` touches; only the lower bound is pinned here.
    if len(fields) < EXPECTED_LENGTH:
        print(
            f"FAIL: per-offset count {len(fields)} < "
            f"block length {EXPECTED_LENGTH}",
            file=sys.stderr,
        )
        return 1
    dual = sorted(off for off, data in fields.items() if len(data["bases"]) == 2)
    if len(dual) < EXPECTED_LENGTH:
        print(
            f"FAIL: only {len(dual)} offsets are dual-base, expected at "
            f"least {EXPECTED_LENGTH} (the whole contiguous block)",
            file=sys.stderr,
        )
        return 1
    print(
        f"Step 2: infer_structs roll-up shows {len(fields)} distinct "
        f"offsets across both bases, {len(dual)} of them dual-base "
        f"(block contributes {EXPECTED_LENGTH})"
    )

    # --- Step 2 (dual-base): confirm the block promotes ----------
    rows = {row["offset"]: row for row in f.top_fighter_offsets(60)}
    if EXPECTED_OFFSET not in rows:
        print(
            f"FAIL: top offsets roll-up missing {EXPECTED_OFFSET}",
            file=sys.stderr,
        )
        return 1
    base_count_01680 = rows[EXPECTED_OFFSET]["base_count"]
    if base_count_01680 != EXPECTED_BLOCK_BASE_COUNT:
        print(
            f"FAIL: {EXPECTED_OFFSET} base_count {base_count_01680} != "
            f"expected {EXPECTED_BLOCK_BASE_COUNT}",
            file=sys.stderr,
        )
        return 1
    print(
        f"Step 2 (dual-base): {EXPECTED_OFFSET} base_count="
        f"{base_count_01680} (PROMOTED - same offset, same width, same "
        "guest IPs from both fighter bases)"
    )

    # --- Step 3 (taint): skipped; documented in the per-slice note
    print(
        "Step 3 (taint): skipped (0x2399c is a store-quad, not a "
        "branch IP; see p1_taint_0x1680_block_v0730.md)"
    )

    print()
    print(
        "PASS: P1 factory chain reproduced on real corpus; "
        "0xd00..0xee0 block stable across both traces and DUAL-BASE "
        "(promoted)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())