# `fa_coli` whole-task bilateral scan mixes — recovered (v0693)

The bilateral mixes with fighter-0 scan 2/6 and fighter-1 scan 1/4 are
now native. The recovery is a pure gate-scope extension justified by
complete instruction-stream identity: no new accounting constant, no
child change, no whitelist change.

## Measured reference (reproduced)

Parked snapshot `out/coli-parked-221e8.vf2snap`, stopping at `0x00010dcc`
(both fighter bit-8 flags set, `field_0804 = 0`, `field_0820 = 0`,
`0x005149cc = 0xffff`):

| F0 scan | F1 scan | `field_0822` (F0) | reference |
| --- | --- | --- | --- |
| 2 | 1 | 0, 1, 16, 256 | **9527 / 18 / 19** |
| 2 | 4 | 0, 1, 16, 256 | **9527 / 18 / 19** |
| 6 | 1 | 0, 1, 16, 256 | **9524 / 18 / 19** |
| 6 | 4 | 0, 1, 16, 256 | **9524 / 18 / 19** |

All 16 cells match the corresponding F1-scan-0 shape exactly
(`9527` for F0 scan 2, `9524` for F0 scan 6), field-independently.

## Trace attribution

Full `--trace` runs were diffed instruction by instruction against the
admitted counterparts:

- bilateral (6,1) vs (6,0): 9524 steps each, **0 divergent steps**;
- bilateral (6,4) vs (6,0): 9524 steps each, **0 divergent steps**;
- bilateral (2,1) vs (2,0): 9527 steps each, **0 divergent steps**;
- bilateral (2,4) vs (2,0): 9527 steps each, **0 divergent steps**.

The mechanism is visible at `0x2232c`: `cmpibe 6` / `cmpibe 5` both
fall through and `cmpibne 2` is taken for F1 scans 0, 1 and 4 alike,
so the first `0x22298` call walks the identical 16-step scan-0 quick
tail in all three compositions. The reference cannot distinguish these
shapes, therefore neither may the native accounting.

## Recovery

- `vf2_hybrid_coli_23524_execute` (`src/recovered/hybrid.c`): the
  measured bilateral `-2` correction gate now accepts
  `scan_821_other == 0 || == 1 || == 4`. The single-live sub-clause is
  untouched (inactive-fighter scans other than 0 remain unmeasured).
- No child, contact, tail or whitelist change: the admitted counts
  (`9527`, `9524`) and triples (`18/19`) already exist.

## Proof

`tests/recovered/test_coli_whole_task_live.c` drives all 16 cells
through the ROM-backed differential
(`vf2_coli_whole_task_live_differential`): the reference re-measures
the counts on every run and native matches counts plus complete
CPU/condition/procedure/Model 2A live state at `0x10dcc`.

Unmeasured compositions (F1 scans 2/5/6 in these bilateral slots, the
symmetric F0-scan-1/4 column, nonzero scans on inactive fighters)
remain fail-closed.
