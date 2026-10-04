# P1 taint step: 0x1680 block does not feed fighter-flag branches (v0730)

This note captures the **second factory-chain step** for P1
(`fa_player` corridor downstream of `0x28918`/`0x29414`):
running `taint.py` on the existing player trace (`out/trace-both.jsonl`)
and characterising the branches downstream of the 0x1680 contiguous
block (offset 0x1680..0x1860, IPs 0x2399c + 0x23a38).

This complements the first P1 step
(`decomp/i960/notes/p1_player_0x1680_block_stability_v0730.md`),
which confirmed the block is structurally stable across both traces.

## Result: negative (and meaningful)

The 0x1680 block stores fighter data, but **none of the branches
downstream of the block carry fighter-flag taint**. The block is
therefore not a gameplay-state field; it is a render/pose/animation
descriptor (or similar non-branch-driving data) per the v0729
"per-frame dispatch boundary" hypothesis.

## Measured branch set (downstream of 0x2399c / 0x23a38)

`taint.py` was run against `out/trace-both.jsonl` with the
existing `out/state8-posbit6-v0727.json` scenario (fighter0
= 0x510000, fighter1 = 0x520000, window 0x2000). The full run
produced **70 branches** total; the 5 branches in the
`0x23990..0x23a80` window (immediately downstream of the block
load/store IPs) are:

```
branch 0x000239a8 depends on: []      ; no fighter taint
branch 0x00023a10 depends on: []      ; no fighter taint
branch 0x00023a2c depends on: []      ; no fighter taint
branch 0x00023a54 depends on: []      ; no fighter taint
branch 0x00023a6c depends on: []      ; no fighter taint
```

All five have empty dependency lists — they do not derive their
condition state from any tagged fighter byte.

## Why `--until 0x2399c` returns no output

`taint.py`'s `--until` filter (`taint.py:316`) limits the printed
branch set to `branch_deps[args.until]`. IPs `0x2399c` and `0x23a38`
are **store-quad** (`stq`) and **load-quad** (`ldq`) instructions
respectively (verified by inspecting trace records). They are
*memory-access* IPs, not *branch* IPs, so they are not in the
`branch_deps` map and `--until 0x2399c` produces an empty report.

This is consistent with the v0729 "per-frame dispatch boundary"
hypothesis: `stq` writes the block (120 offsets × 4 B = 480 B of
fighter state in a single instruction), `ldq` reads it back. Neither
is a control-flow decision.

## What the taint output DID surface (for context)

The full taint run on `out/trace-both.jsonl` did surface
fighter-flag dependencies on the **0x1a4** byte (per the AGENTS.md
next-work #3 contract `branch 0x00018698 depends on:
fighter0 + 0x1a4 bit 6`). The earliest measured dependencies in the
trace are:

```
branch 0x000222a8 depends on: fighter0 + 0x01a4 bit 8
branch 0x000222ac depends on: fighter0 + 0x01a4 bit 1
branch 0x000222b0 depends on: fighter1 + 0x01a4 bit 14
branch 0x000223c4 depends on: fighter0 + 0x01a4 bit 0
                              fighter1 + 0x05b8 bit 0
branch 0x00022420 depends on: fighter0 + 0x01a4 bit 8
                              fighter1 + 0x01a4 bit 8
...
```

These are *gameplay-state* flag branches. They feed the
`fa_player` / `fa_coli` decision code. They are **not** fed by the
0x1680 block, confirming the 0x1680 block is a separate category.

## Conclusion

The 0x1680 contiguous 4B block (length 120, 480 B, IPs 0x2399c +
0x23a38) is **structurally stable** (P1 first step) but **does not
feed fighter-flag gameplay branches** (this step). It is a render /
pose / animation-state / per-frame dispatch descriptor.

Implications for the recovered C semantics:

- The 0x1680 block must be modeled in C (the recovered struct has
  120 dwords at offset 0x1680..0x1860), but those fields should
  not be promoted to gameplay-state semantics like `health`,
  `animation_state`, or `combo_counter` without independent
  evidence.
- Branch decisions in `fa_player` / `fa_coli` are driven by
  offsets in the 0x1a4 / 0x5b8 / 0xb24 byte cluster, not by the
  0x1680 block. The block is a sibling cluster, not a decision
  input.

## Factory chain composition for P1 (cumulative)

This step uses:

- `tools/python/taint.py` (v0729b, v0729c) — the same
  `branch <ip> depends on:` contract applied to the player trace.
- 7/7 unit suite + 1/1 E2E on the real corpus
  (`tools/python/test_taint.py`, `tools/python/test_taint_e2e.py`)
  is the regression net.
- The same public API the v0729c E2E used; no new tool surface
  was introduced.

Combined with the first P1 step:

- **Step 1** (`p1_player_0x1680_block_stability_v0730.md`):
  block is structurally stable across both traces.
- **Step 2a** (this note): block does not feed fighter-flag
  branches; it is a render / pose / per-frame descriptor.
- **Step 2** (next, when scheduled): run `infer_structs.py`
  dual-base promotion on a fighter1-inclusive trace (currently
  unavailable — would need `out/trace-f1.vf2snap` or similar).

## Reference reading order

1. `decomp/i960/notes/v0730_first_action_runbook.md` — single
   entry-point.
2. `decomp/i960/notes/factory_runbook_v0729.md` — 5-step playbook
   + 12-suite gate.
3. `decomp/i960/notes/p1_player_0x1680_block_stability_v0730.md`
   — first P1 step (block is structurally stable).
4. This note — second P1 step (block is not a gameplay-state
   branch input).

## Anti-traps

- Do not promote `field_0x1680`, `field_0x1700`, …, `field_0x185c`
  to semantic names like `pose`, `animation_state`, or
  `render_buffer` without independent evidence.
- Do not commit large JSON dumps of the taint output; reproduce
  from the command documented above.
- Do not weaken `taint.py`'s conservative branch-dependency shape
  (the AGENTS.md next-work #3 contract is the durable evidence).
- Do not confuse `--until <ip>` with "show me everything that
  *depends on* this IP" — `--until` filters branches *at* that
  IP. The correct query for "show me everything that depends on
  this load" is to find the load's downstream branches by hand
  (as this note does).