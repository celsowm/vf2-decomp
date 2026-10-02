# `fa_coli` single-live F1 scan-2 — measured fail-closed attribution (v0696)

The single-live inactive-scan sweep (Phase 2) admitted every measured
cell except one: single-live F1 with active scan 2. That shape stays
**fail-closed** with a reference-exact pin. The +1 mechanism is fully
attributed below; a surgical correction would be curve-fitting, and the
honest fix is specified as a joint refit.

## Measured reference (reproduced)

Parked snapshot `out/coli-parked-221e8.vf2snap`, stopping at
`0x00010dcc` (`field_0820 = 0`, `0x005149cc = 0xffff`,
`field_0822 = 0` both sides):

| live side | active scan | inactive scan | reference |
| --- | --- | --- | --- |
| F0 | 0/1/3/4 | 0..6 | 9385 / 17 / 18 (all) |
| F0 | 2 | 0/6 | 9392 / 17 / 18 |
| F0 | 5 | 0/5/6 | 9391 / 17 / 18 |
| F0 | 6 | 0/2/6 | 9389 / 17 / 18 |
| F1 | 0/1/3/4 | 0..6 | 9385 / 17 / 18 (all) |
| F1 | 2 | 0/6 | **9392 / 17 / 18** |
| F1 | 5 | 0/5/6 | 9391 / 17 / 18 |
| F1 | 6 | 0/2/6 | 9389 / 17 / 18 |

The inactive fighter's `field_0821` is fully inert in every
single-live shape: identical absolute counters, not just equal
totals. No native single-live gate constrains it, so all cells but
one assert native equality plus complete live-state equality
through the ROM-backed differential.

## Trace attribution of the F1-single-2 +1

Full `--trace` runs of F1-single-2 vs F1-single-0 were split at the
two `call 0x22298` instructions and at midbody entry `0x22210`:

| section | F1s0 ref | F1s2 ref | F1s0 nat | F1s2 nat |
| --- | --- | --- | --- | --- |
| pre-shell (0x221e8) | 7 | 7 | 7 | 7 |
| shell (0x23524 region) | 9298 | 9298 | 9298 (body 9297+ret) | 9298 (body 9297+ret) |
| midbody children (excl. rets) | 14+7=21 | 21+6=27 | 13+6=19 | 21+6=27 |
| midbody rest | — | — | 57+rets | 57+rets |
| final dispatch compare | — | — | +1 | +1 |
| **total** | **9385** | **9392** | **9385** | **9393** |

Tails after the call-2 `ret` are instruction-identical (55 steps);
shells are identical (9298 both). The whole +7 reference step lives
in call 1 (scan-0 quick tail 15 in-call incl. `ret` vs scan-2 fail
tail 22 incl. `ret`).

Native undercounts the scan-0 fall-through child by 1 (body 13 vs
14 pre-`ret` insns — the v0696 scan-0 decomposition) and overcounts
the single-live rest by 1 (dispatch compare). The two cancel for
every scan-0-shaped single, but the scan-2 F1 single has exact
children, so the rest +1 surfaces as 9393 vs 9392. The F0-single-2
mirror passes only because the F0-special net -1 shell correction
happens to compensate there.

A shell -1 scoped to F1-single-2 would be curve-fitting on
trace-identical shells (9298 both). The honest fix is the joint
refit: scan-0 child body 13->14, single-live final +1->0,
rebalanced F0-special/bilateral gates, full-matrix re-proof.

## Recovery

- `vf2_hybrid_coli_23524_execute` (`src/recovered/hybrid.c`):
  single-live F1 scan 2 now returns `VF2_ERROR_UNSUPPORTED` at the
  admission gate, so the whitelisted 9393 total of the admitted
  F0-single-2 shape cannot accept this composition.
- `tests/recovered/test_coli_whole_task_live.c`: the two measured
  F1-single-2 cells (inactive 0/6) pin reference-exact counts plus
  fail-closed behavior; all other measured single-live
  inactive-scan cells assert native equality.

## Proof

`vf2_coli_whole_task_live_differential`: every driven cell asserts
its rule-derived reference triple; the F1-single-2 pin asserts
`VF2_ERROR_UNSUPPORTED` natively.
