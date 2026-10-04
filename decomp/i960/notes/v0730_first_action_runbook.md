# v0730 first action: how to use the v0729 factory layer (runbook)

This note is the **single** entry point for the next agent or
session that picks up the project. It assumes `master` is at
`v0729p` (or later) with the factory layer committed in this
session (38+ commits, 87 Python test cases, 12 ctest entries).

## TL;DR

```sh
# 1. Verify the factory gate is green on this checkout.
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug -R vf2_python_factory --output-on-failure
# Expected: 12/12 tests passed (~7 s).

# 2. Pick the next F slice (TEST MENU completion) or P slice
# (fa_player corridor). Both have a documented runbook.

# 3. Run the factory chain demo for a smoke check.
python tools/python/factory_chain_demo.py
```

If step 1 fails, do not start any new recovery work; fix the factory
gate first.

## What's on master

- **v0729 factory layer** (Layer 2 of the v0729 plan, fully delivered):
  - `tools/python/frontier.py v2` (per-edge fighter offsets, per-source
    attribution, fighter-aware rank, contiguous-block detection,
    `--json` surface)
  - `tools/python/taint.py` (unit + E2E on real corpus)
  - `tools/python/infer_structs.py` (dual-base promotion)
  - `tools/python/infer_rules.py` (conservative refusal)
  - `tools/python/check_scenario.py` (scenario validation)
  - `tools/python/sweep_state.py`, `explore_state.py`,
    `minimize_case.py`, `trace_case.py` (workflow helpers)
  - `tools/python/z3_branch.py` (AGENTS.md next-work #5)
  - `tools/python/test_factory_chain.py` (integration: Step 1
    -> 2 -> 2a + infer_rules composition)
  - `tools/python/factory_chain_demo.py` (runnable exemplar)

- **Tests**: 87 Python test cases across 12 ctest entries
  (`#105`-`#116`), 7.45 s wall. C build clean.

- **Evidence notes** in `decomp/i960/notes/`:
  - `factory_runbook_v0729.md` (5-step playbook + 12-suite gate)
  - `frontier_v2_player_corpus_v0729.md` (smoke evidence)
  - `fa_player_struct_1680_corpus_v0729.md` (the **measured P1
    entry**: contiguous 0x1680 4B block, length 120, 480 B,
    ip_overlap 1.0, touched by IPs `0x2399c` + `0x23a38`)
  - `v0729_session_close_v0730_handoff.md` (older session
    close-out summary)

## What's still deferred (multi-week ROM-backed recovery)

Per `docs/UNCOVERED_BRANCHES.md` and `decomp/i960/notes/v0727`:

### Layer 0 — TEST MENU completion (F1-F5)

Each slice: 30–90 minutes of focused RAM-resident work plus a
strict `vf2cmp|native-*` differential per the v0727 playbook.
The v0727 "Still open" list is the work queue:

1. **F1**: MANUAL SETTING natural entry (selector 17 index 4) and
   the whole nested a7 editor.
2. **F2**: Rows 2-4 post-edit release frames (post-edit states with
   credit index != 2, derived credits, preset != 0). Requires
   value-driven digit renders for rows 6-9/11 and a measured gate
   widening.
3. **F3**: PUNCH/KICK releases at value rows + KICK -1 from
   INDIVIDUAL mode.
4. **F4**: INDIVIDUAL-mode walks at rows 2-4.
5. **F5**: Bump `native-seventh-dispatch` boundary past v0727.

### Layer 1 — fa_player corridor downstream (P1-P4)

1. **P1**: 0x29414 nonzero entry + dual-base struct upgrade. The
   measured entry evidence is already on master in
   `fa_player_struct_1680_corpus_v0729.md` — start by running
   `infer_structs.py` against the dual-trace player corpus,
   confirming the `0x1680..0x1860` block (length 120) is stable
   across both traces. Then run `taint.py --until 0x2399c` to
   characterise the dependent branch.
2. **P2-P4**: Decomposition past `0x28918`/`0x29414` per the
   runbook chain.

### Layer 3 — frame-dispatch selector 3 phase ≥8 (`0x9444`)

Explicitly outside the v2 factory. Requires its own measurement
probe. Documented in `NATIVE_RUNTIME.md` "Next integration".

## How the factory composes for the next slice

For an F-slice (TEST MENU completion):

1. Read the v0727 note to identify the boundary.
2. Use the existing `out/sixth-fresh.vf2snap` as the entry point.
3. Run `vf2i960 native-resume --snapshot out/sixth-fresh.vf2snap
   --set-u32 ...` to capture a boundary snapshot.
4. Apply the existing `decomp/i960/tools/make_game_info_probe_scenario.py`
   plus `tools/python/sweep_state.py` + `tools/python/infer_rules.py`
   to find the compact rule.
5. Translate the rule to a recovered C block + CTest pin.
6. Run the strict differential; commit the slice + per-slice note.

For a P-slice (fa_player corridor):

1. Use `tools/python/factory_chain_demo.py` as a starting template.
2. Run `tools/python/frontier.py --fighter-base 0x510000
   --fighter-base 0x520000 out/trace-both.jsonl out/trace-f0.jsonl
   --json` and inspect the resulting `--json` output for the
   `0x1680` block.
3. Run `tools/python/infer_structs.py --base fighter0=0x510000
   --base fighter1=0x520000` on a fighter1-inclusive trace once one
   exists (currently the dual traces hit only fighter0).
4. Run `tools/python/taint.py --rom-dir roms/vf2
   --vf2i960 build/Debug/vf2i960.exe --scenario
   out/state8-posbit6-v0727.json --trace <trace> --until 0x2399c` for
   the dependent-branch contract.
5. Translate to a recovered C block + CTest pin + per-slice note.

In both cases, every step is gated by `tools/python/factory_chain.py`
(v1) or `factory_chain_demo.py` (v0); the runbook is the durable
contract.

## If a tool fails its ctest

The 12 factory ctest entries are the regression net. If a ctest
entry fails:

1. Do not commit unrelated changes.
2. Read the failing test in `tools/python/test_*.py` to identify
   which contract the implementation regressed.
3. Fix the implementation in `tools/python/<tool>.py` (not the
   test) unless the test is genuinely wrong.
4. Re-run the ctest entry alone:
   `ctest --test-dir build -C Debug -R vf2_python_factory_<tool>
   --output-on-failure`.
5. Once the offending test is green, re-run the full factory gate
   before any new commit.

## Reference documents (in order of reading priority)

1. `AGENTS.md` — project conventions, build/test gate, "Current
   handoff status" (last updated for v0729), "Recommended next
   work".
2. `docs/UNCOVERED_BRANCHES.md` — exact open frontiers per slice.
3. `decomp/i960/notes/factory_runbook_v0729.md` — 5-step playbook
   + 12-suite gate.
4. `decomp/i960/notes/v0729_session_close_v0730_handoff.md` —
   session close-out + measured entry evidence.
5. `decomp/i960/notes/v0727_coins_assignment_natural_v0727.md`
   (or `playable_coin_assignment_natural_v0727.md`) — playbook
   template for F-slice work.
7. `decomp/i960/notes/fa_player_*_v0707.md` and
   `fa_player_downstream_dual_base_v0706.md` — playbook template
   for P-slice work.

## Anti-traps

- Do not hand-write a third mask table in `fa_game_info` or
  `fa_coli`. The v0728 sweep proved the table path is the wrong
  default; the counted-rule path is the discipline.
- Do not weaken `infer_rules.py`'s refusal contract. It is exactly
  the discipline that retired the v0728 15-entry triple/quad table.
- Do not promote `field_xxxx` to a semantic name (e.g. `health`,
  `animation_state`) without independent evidence.
- Do not commit `.vf2snap`, large JSONL, or other derived
  proprietary artifacts. `out/` is gitignored.
- Do not move game semantics into the i960 interpreter to avoid
  recovering them in C. The executor is the oracle; the recovered
  C is the implementation.