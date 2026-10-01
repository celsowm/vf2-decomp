# `fa_player` `0x19ef8` measured-mask family audit (v0690)

## Outcome

The 49-value `state_ok` enumeration is now a declared table
(`vf2_hybrid_player_19ef8_measured_masks` in `src/recovered/hybrid.c`)
instead of a 49-branch `||` chain, with the identical accept/reject set
(proven by extraction: both lists contain the same 49 values in the same
order, and no `||` branch remains in the guard).  Nothing about acceptance
changed: up to three non-branch bits still compose freely, each larger
shape is still one individually measured row, and everything else stays
fail-closed.

## Findings that blocked a "compact rule" (A3 residual)

The accepted set is **non-monotone**, measured, so no
subset/bitmask predicate of the form `mask & ~S == 0` can reproduce it and
no compact rule is derivable from the current evidence:

- `0x0000059f` is admitted, while its strict subset `0x0000011f` is not.
  Measured bit 8 (`0x0000009f`) selects a ROM branch that `0x0000001f`
  alone does not, so acceptance does not follow set inclusion.
- The reserved control bit 12 (`0x00001000`) appears in no row; `row | bit12`
  is rejected for all 49 rows (script-verified: none of the 49 supersets
  is listed and each has popcount > 3).

Consolidating the enumeration into data is therefore the honest recovery;
a hand-written compact predicate would over-accept by construction.

## Stale negative controls repaired (2 + 1 red shapes)

The previous negative-control chains were inconsistent with the measured
acceptance set:

- `--six-low` (mask `0x9f`) failed: its control added bit 8, which yields
  `0x19f`, measured and admitted since the `mask_19f` slice.
- `--seven-low` (mask `0x19f`) failed: its control added bit 10, which
  yields `0x59f`, measured and admitted since the `mask_059f` slice.
- The default ROM-backed sweep drove `0x1f` while expecting
  `VF2_ERROR_UNSUPPORTED`; `0x1f` was admitted as measured (five-low
  slice), so that tail control was stale and the differential could not
  finish green.

All three now drive `mask | bit12`, which is genuinely unmeasured.  The
ROM-backed runs confirm: `--six-low`, `--seven-low` and the new
`--mask-family` mode (49 rows x 16 branch subsets + 49 controls) all pass.

## Coverage added

- `--mask-family` drives every measured row with all sixteen combinations
  of branch bits 5/6/21/23 plus the reserved-bit control, in one
  process; registered as `vf2_player_4505_mask_family_differential`.
- The ROM-independent unit test (`test_unit_mask_family_table`, wired
  into the binary's no-arg run) pins the enumeration's shape: exactly 49
  duplicate-free rows, no branch bits, no control bit, each row admitted
  and each `row | bit12` rejected, plus the `0x11f`/`0x59f` non-monotone
  witness and the out-of-range `at()` sentinel.
- Test-only C accessors
  (`vf2_hybrid_player_19ef8_mask_admitted_for_test`,
  `..._measured_mask_count_for_test`,
  `..._measured_mask_at_for_test`) expose the recovered enumeration to
  that unit test without touching the corridor's memory contract.

## What remains

- Row notes: each of the 49 rows already has a
  `fa_player_19ef8_mask_*` / `fa_player_19ef8_*low` note; the audit does
  not replace them.
- New shapes must still be measured individually (ROM probe + branch
  matrix) before joining the table, and the fixture's literal
  `measured_mask_family_rows` must grow in the same slice or the unit
  test fails.
- Sibling recovery (fighter physics, `0x2704c` replay fault, `0x19ef8`
  post-selector semantics) is unchanged by this work.
