# `fa_coli` field_0820 != 0 section battery (v0699, scouting + preservation)

The v0698 note left the `field_0820 != 0` world as a follow-up slice.
This note runs its section battery for the two exercised shapes
(test_one `f0` and `both`), decomposes the reference sections, and maps
each to the native accounting. No recovery change in this slice: both
shapes were already exact and stay exact; the battery promotes their
accounting from "preserved pin" to "section-decomposed", and names the
one residual honestly.

Method: parked snapshot `out/coli-parked-221e8.vf2snap`, whole-task
`--trace` runs to `0x00010dcc` with the test_one recipes
(`0x00510b24/0x00512b24 = 0x100`, F0 `0x005111a0 = 1`,
`0x005149cc = 0xffff`), split at the `call 0x22298` sites
(0x22210/0x2221c) and the `call 0x22404` sites (0x22228/0x22238).
Snapshot defaults are all zero (probed `0x005111a0/0x005131a0/
0x00510d84/0x00512d84/0x00510b24/0x00512b24`).

## Reference section table (820 != 0 vs matrix 820 == 0)

| shape | total | 2298 regions | 2404 regions | non-4-call |
| --- | --- | --- | --- | --- |
| F0-live single, 820=1, scans 0/0 | 9393 | 8 / 16 | 37 / 15 | 9369 |
| both-live, F0_820=1, scans 0/0 | 9528 | 16 / 16 | 37 / 31 | 9496 |
| matrix F0-live scan 0 (820=0) | 9385 | 8 / 16 | 31 / 15 | 9361 |
| matrix bilateral (0,0) (820=0) | 9520 | 16 / 16 | 31 / 31 | 9488 |

So `F0_820 = 1` costs +8 over the matrix analogs: +6 inside the
F0-side 0x22404 call (region 37 vs 31; listings: the 820=1 call takes
a longer contact path, both-live and single alike) and +2 in the
shell/rest/dispatch remainder. The 2298 regions are the standard
scan-0 tails (warm 8, quick 16); every scan delta still lives in the
calls. The single-live inactive scan stays inert (not re-swept here).

## Native correspondence (measured with temporary section prints, reverted)

- `f0`-mode: children 6/14 (regions 8/16, exact), 2404a body 35
  (region 37, size-exact vs the reference 37-region), 2404b 13
  (region 15, exact). Shell 9299, no gates (820 != 0 excludes all of
  them), no dispatch addition. Total 9393/17/18 + live-state equality.
- `both`-mode: takes the measured both-live 110-tail early return in
  `vf2_hybrid_coli_midbody_tail_execute` (gate: four threshold
  halfwords zero + `b820 = 1/0`, body 107, 4 calls / 4 rets — the
  reference tail is exactly recs 9418->9528 = 110 with 4 calls), with
  the six halfword + three byte stores replicated, plus the preserved
  both-live +2 (820 != 0). Total 9528/18/19 + live-state equality.

## Residual (explicit, not claimed)

The 2404a body-35 native path is proven in size (37 region) and state
(differential green) but its internal path-vs-reference listing diff
was not diffed instruction-by-instruction here; likewise the both-mode
110-tail body 107 vs the reference tail's internal call/region split.
Both shapes keep their long-green pins with full triple + live-state
proof. A future slice can diff those two regions the way v0696
diffed the scan-0 child, but nothing in the totals, calls, rets, or
mutable state observed here justifies refusing them.

## Proof

`vf2_coli_whole_task_live_differential` (all test_one modes incl.
`f0` 9393 and `both` 9528, full matrix) green; no source change.
