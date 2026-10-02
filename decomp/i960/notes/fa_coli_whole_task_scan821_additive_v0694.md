# `fa_coli` whole-task additive scan-821 rule — measured grid, scan-3 admission (v0694)

The full bilateral `field_0821` scan matrix is now measured, the whole
matrix fits a compact additive per-side rule with zero misses, and the
scan-3 compositions are native (gate-scope extension on proven
instruction-identical streams). The remaining unmeasured-behavior
compositions are pinned fail-closed with their exact reference counts.

## Measured reference (reproduced)

Parked snapshot `out/coli-parked-221e8.vf2snap`, stopping at
`0x00010dcc` (both fighter bit-8 flags set, `field_0804 = 0`,
`field_0820 = 0`, `0x005149cc = 0xffff`). All 48 bilateral scan pairs
`0..6 x 0..6` (except 6/6, pinned separately) crossed with the
`field_0822` sweep `{0, 1, 16, 256}`, plus the single-live scan-3
witnesses on both sides — 502 measured cells.

Bilateral totals fit `9520 + delta(F0 scan) + delta(F1 scan)` with
`delta = {2: +7, 5: +6, 6: +4, 0/1/3/4: +0}` and single-live totals fit
`9385 + delta(active scan)` for **every measured cell** (0 misses),
field-independently. This replaces the per-shape hand table in the test
with the measured compact rule (`coli_scan_821_delta`).

Mechanism: each side's delta is its opponent-keyed contribution at the
`0x2232c` three-test shortcut (the second `0x22298` call region): scans
0/1/3/4 take the quick tail, scan 2 walks the full 11-step block, scan 5
skips only the `cmpibne 2` test and scan 6 joins the block mid-way.

## Trace attribution

Full `--trace` runs were diffed instruction by instruction against the
admitted scan-0 counterparts:

- single-live F0 scan 3 vs scan 0: 9385 steps each, **0 divergent
  steps**;
- single-live F1 scan 3 vs scan 0: 9385 steps each, **0 divergent
  steps**;
- bilateral (3,0) vs (0,0): 9520 steps each, **0 divergent steps**;
- bilateral (2,3) vs (2,0): 9527 steps each, **0 divergent steps**.

The reference cannot distinguish scan 3 from scan 0 on either side, and
cannot distinguish F1 scan 3 from F1 scan 0 under an F0 scan-2 shape,
therefore neither may the native accounting.

## Recovery

- `vf2_hybrid_coli_23524_execute` (`src/recovered/hybrid.c`): the
  measured quick-tail `-1` gates now accept `scan_821 == 3` on the F0
  side (single-live and bilateral), and the measured F0-scan-2/6 `-2`
  gate now accepts `scan_821_other == 3`. No new constant, no child
  change, no whitelist change: the admitted counts (`9385`, `9520`,
  `9524`, `9526`, `9527`) and triples already exist.
- Same function: new explicit fail-closed gate for the two bilateral
  compositions whose previous accounting produced wrong-but-whitelisted
  totals — `(F1 scan 2, F0 scan in {0,1,3,4})` counted 9528 vs measured
  9527, and `(6,5)` counted 9532 vs measured 9530. Without the missing
  trace-backed opponent-side corrections those totals would be accepted
  against the wrong shapes, so both stay `VF2_ERROR_UNSUPPORTED`.
  All other unproven compositions fail closed naturally (their native
  counts are not whitelisted).

## Proof

`tests/recovered/test_coli_whole_task_live.c` drives all 502 cells
through the ROM-backed differential
(`vf2_coli_whole_task_live_differential`). For trace-admitted
compositions (including the newly admitted scan-3 singles, the bilateral
scan-3 mixes and (2,3)/(6,3)) native matches reference counts plus
complete CPU/condition/procedure/Model 2A live state at `0x10dcc`. For
pinned compositions the test asserts the exact rule-derived reference
counts AND `VF2_ERROR_UNSUPPORTED`, mirroring `test_bilateral_66_unsupported`.

## Remaining fail-closed frontier (all with exact measured reference counts)

- `(F0 scan in {0,1,3,4,5}, F1 scan in {2,6})` — needs the opponent-side
  scan-2/6 shell correction (`-1` parity with the measured F1 scan-5
  witness), not yet traced;
- `(F0 scan in {2,6}, F1 scan in {2,5,6})` — needs the double-special
  shell corrections, not yet traced;
- `(6,6)` — measured `9528/18/19`, unchanged fail-closed pin (v0692).
