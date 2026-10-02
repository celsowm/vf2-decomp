# `fa_coli` whole-task residual scan mixes — recovered (v0695)

The v0694 fail-closed residual bilateral frontier — opponent-side scan
2/6 mixes without an F0-side special, and the double-special mixes — is
now native. The recovery is two pure accounting-parity gate extensions
on proven instruction-identical shells: no new constant, no child
change. After this slice, the entire measured bilateral `field_0821`
matrix (`0..6 x 0..6`, minus the v0692-pinned 6/6) is recovered under
the additive per-side rule.

## Measured reference (reproduced)

Parked snapshot `out/coli-parked-221e8.vf2snap`, stopping at
`0x00010dcc` (both fighter bit-8 flags set, `field_0804 = 0`,
`field_0820 = 0`, `0x005149cc = 0xffff`), `field_0822 = 0` (all counts
field-independent across `{0, 1, 16, 256}`, verified by the v0694 grid):

| F0 scan | F1 scan | reference |
| --- | --- | --- |
| 0 | 2 | **9527 / 18 / 19** |
| 0 | 5 | **9526 / 18 / 19** |
| 0 | 6 | **9524 / 18 / 19** |
| 5 | 2 | **9533 / 18 / 19** |
| 5 | 5 | **9532 / 18 / 19** |
| 5 | 6 | **9530 / 18 / 19** |
| 2 | 2 | **9534 / 18 / 19** |
| 2 | 5 | **9533 / 18 / 19** |
| 2 | 6 | **9531 / 18 / 19** |
| 6 | 0 | **9524 / 18 / 19** |
| 6 | 2 | **9531 / 18 / 19** |
| 6 | 5 | **9530 / 18 / 19** |

Every cell equals the v0694 additive rule `9520 + delta(F0 scan) +
delta(F1 scan)` with `delta = {2: +7, 5: +6, 6: +4, 0/1/3/4: +0}`.

## Trace attribution

Full `--trace` runs were aligned instruction by instruction against the
admitted counterpart shells. Conviction requirement: divergence must be
confined to a `0x22298` call region, and the window step deltas must
equal the measured per-side rule deltas.

Opponent-side parity (F1 scans 2/6 vs the admitted F1 scan 5 witness;
divergence inside the first `0x22298` call only):

- (0,2) vs (0,5): +1 step at `0x22334` (scan 2 walks the extra
  `cmpibne`-block test), all else identical;
- (0,6) vs (0,5): -2 steps (scan 6 joins the block mid-way at
  `0x2233c`), all else identical;
- (5,2) and (5,6) vs (5,5): identical +1 / -2 pattern.

Double-special shells (F0 special fixed, opponent special varying;
divergence inside the first `0x22298` call only):

- (2,2) vs (2,0): +7 = delta(scan 2);
- (2,5) vs (2,0): +6 = delta(scan 5);
- (2,6) vs (2,0): +4 = delta(scan 6);
- (6,2) vs (6,0): +7; (6,5) vs (6,0): +6.

The reference shells cannot distinguish these compositions outside the
measured child bodies, therefore the native accounting may not either.

## Recovery

- `vf2_hybrid_coli_23524_execute` (`src/recovered/hybrid.c`):
  - the measured bilateral F1-special `-1` correction gate now accepts
    opponent scans `{2, 5, 6}` (was `{5}` only);
  - the measured F0-special `-2` correction gate drops its opponent-set
    restriction (any F1 scan value in the bilateral shape);
  - the v0694 measured-matrix strictening gate is replaced by the
    permanent bilateral 6/6 admission gate: v0692 measured the 6/6
    reference at `9528/18/19` with a 20-step scan-6 ordering-fail tail
    in the first `0x22298` call that the recovered child does not model
    (native account 9531 pre-gate). Refusing 6/6 at the gate is now
    load-bearing: the whitelisted 9531 total of the admitted (2,6)/(6,2)
    mixes would otherwise accept the unmatched composition.
- Whole-task whitelist (`vf2_hybrid_first_dispatch_task_execute`):
  admits the newly reached totals `9530`, `9531`, `9533`, `9534`
  (all 18/19).

## Proof

`tests/recovered/test_coli_whole_task_live.c` drives the full grid
through the ROM-backed differential
(`vf2_coli_whole_task_live_differential`): every measured cell asserts
the rule-derived reference counts, native equality of counts, and
complete CPU/condition/procedure/Model 2A live-state equality at
`0x10dcc`; the single 6/6 composition keeps its
`test_bilateral_66_unsupported` reference-exact + fail-closed pin.

## Remaining fail-closed frontier

- Bilateral 6/6 (`9528/18/19` reference): needs the measured 20-step
  scan-6 ordering-fail child tail, not accounting parity (v0692).
- Single-live shapes with nonzero scans on the inactive fighter remain
  unmeasured and fail-closed.
