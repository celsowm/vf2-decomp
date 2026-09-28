# v0388 — game_info plus2_plus3 / condition-only countdown-0 compare-state fix

## Symptom

The uncommitted working-tree edit in
`src/recovered/hybrid_game_info_poststate_base.inc`
(`correct_measured_compare_state`: stop gating EQUAL on `fighter0_only`,
always EQUAL for countdown 0, LESS otherwise) fixes 4/12 DIFFs on the
`0x06214000` matrix (12 fixtures: 3 distributions × countdown 0/1 × mode6
0/1 × threshold 0). Before the fix the four countdown-0 non-f0-only cells
(f1-only cd=0 m6=0/1, bilateral cd=0 m6=0/1) DIFFed in architectural
condition state; f0-only cd=0 and all cd!=0 cells already matched.

## Root cause

The old code only admitted `fighter0_only && countdown == 0` for EQUAL,
so f1-only and bilateral countdown-0 cases fell through to the generic
tail (which does not set EQUAL). The reference leaves countdown-0 at
EQUAL on all distributions for these two families. The sibling helper
`correct_countdown_compare_state` (same file, lines ~219-225) already
encodes exactly the countdown-keyed EQUAL/LESS rule, confirming the fix
is the measured postcondition, not a guess.

## Scope of the fix

`correct_measured_compare_state` is called only by:

- `measured_plus2_plus3_family` (50 masks, list at lines 76-94) — with
  frame3 poststate + bilateral/unilateral instruction joins;
- `measured_condition_only_family` (12 masks, lines 96-101);
- the single-mask tails `0x00214000` (bit-14+bit-16+bit-21),
  `0x00210000` / `0x00218000` (bit-16 family),
  `0x00204000` (bit-14).

Old vs new differ only when `countdown == 0 && !fighter0_only`
(f1-only and bilateral countdown-0 cells).

## Validation (this slice, ROM-backed, `validate_game_info_full_dispatch.py`)

Each mask: 36 fixtures = 3 distributions × countdown 0/1 × mode6 0/1 ×
thresholds 0,1,2 through the full `0x1645c` dispatcher to `0x10dcc`,
snapshot + arch-signature + counters exact.

- `0x06214000`: 36/36 exact (was 8/12 on the threshold-0 matrix).
- all 50 plus2_plus3 masks: 36/36 exact each (1800 fixtures).
- all 12 condition-only masks: 36/36 exact each (432 fixtures).
- single-mask tails sharing the helper: `0x00204000`, `0x00210000`,
  `0x00214000`, `0x00218000` each 36/36; spot matrix `0x0021c000`
  (member of measured_masks) 36/36 exact.

Strict suite: `ctest -C Debug` 72/72 PASS with the fix built in.

## Status

Native in tree (minimal 3-line change, no new admissions, no fixture or
harness changes). Fail-closed siblings untouched: unmeasured masks and
out-of-range thresholds still return `VF2_ERROR_UNSUPPORTED`.
