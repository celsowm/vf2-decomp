# `fa_coli` whole-task `field_0821 = 6` — recovered (v0691)

The `field_0821 = 6` whole-task shapes measured in v0689 are now native, plus
the fighter-1 single-live scan-6 sibling. The v0689 `+2` question is closed by
instruction-trace attribution: the recovered scan-2 shell correction applies
to scan 6 because the reference executes an instruction-identical stream for
both scans outside the known test shortcuts.

## Measured reference (reproduced)

Parked snapshot `out/coli-parked-221e8.vf2snap`, stopping at `0x00010dcc`:

```sh
build/Debug/vf2probe.exe --rom-dir roms/vf2 \
  --snapshot out/coli-parked-221e8.vf2snap \
  --set-u32 0x00510b24=0x100 --set-u32 0x00510d84=0 \
  --set-u8 0x005111a1=6 --set-u16 0x005111a2=0 \
  --set-u32 0x00512b24=<0|0x100> --set-u32 0x00512d84=0 \
  --set-u8 0x005131a1=0 --set-u16 0x005131a2=0 \
  --set-u32 0x005149cc=0xffff --until 0x00010dcc --trace
```

| shape | fighter flags | f0 `+0x821` | f1 `+0x821` | reference |
| --- | --- | --- | --- | --- |
| single live | `0x100 / 0` | 6 | 0 | **9389 / 17 / 18** |
| both live | `0x100 / 0x100` | 6 | 0 | **9524 / 18 / 19** |
| f1 single live | `0 / 0x100` | 0 | 6 | **9389 / 17 / 18** |
| bilateral 6/6 | `0x100 / 0x100` | 6 | 6 | **9528 / 18 / 19** |

The first two shapes reproduce v0686/v0689 exactly (call/return deltas
against the zero-step baseline `procedure_calls=44717`,
`procedure_returns=44712`). The F1 single and bilateral 6/6 measurements are
new evidence; the 6/6 shape takes the `0x22298` first-call `0xffff`
ordering-fail tail (span 20 vs the scan-0 quick tail's 16, i.e. `+4` with a
different `fighter+0x6dc` store — reference final state differs from 6/0 only
in `fighter0+0x6dc = 0xffff` vs `0` and the `+4` count). The native
composition reaches `9531` for it; the recovered ordering-fail children are
exact for scan 6 (body 18 both calls), so the residual `+3` is the bilateral
shell `-2` correction gate (measured for F1 scan 0 only) plus the pre-existing
one-instruction F1-scan-0 first-call modeling gap that the measured
corrections absorb for the admitted shapes. Stacking a `6/6`-keyed pair of
corrections over exact children would be curve-fitting, so 6/6 remains
**fail-closed** (verified: the native path returns `VF2_ERROR_UNSUPPORTED`
for it; `9531` is no whitelisted triple).

## `field_0822` sweep (field-independent)

The recovered ordering-fail path stores `0xffff` at `+0x6dc` before the
`0x22404` contact sequence, so `field_0822` cannot influence it. Measured:
F0 single and F1 single scan-6 both stay `9389/17/18`, and F0 bilateral
scan-6 stays `9524/18/19`, for `field_0822 ∈ {0, 1, 16, 256}` — the same
field axis the v0683/v0684 matrices cover for scans 0/1/4/5. The ROM-backed
matrix now drives all twelve cells with full live-state equality.

## Trace attribution of the v0689 `+2`

Full `--trace` runs of the scan-6 and scan-2 shapes were diffed for both
single-live and bilateral forms:

- The instruction streams are **identical up to the second `0x22298` call**
  (single: first divergence at step 9,326; bilateral: step 9,445).
- Inside that call the only difference is the measured three-test shortcut:
  scan 6 takes `0x2232c cmpibe 6 -> 0x2233c` directly, skipping
  `0x22330 cmpibe 5` / `0x22334 cmpibne 2` / `0x22338 bbs 12` that scan 2
  walks. Both scans then reach the same `ld/ld/cmpr/bge` ordering check and,
  on the measured zero ordering (`g7+0x1f8 == g7+0x6e4 == 0`), the same
  `0x223a8` `0xffff` tail (`stos r11, 0x6dc(g7)`).
- After resynchronising by three instructions, the remaining streams are
  identical to `0x10dcc` (single: 0/54 mismatches; bilateral: 0/69).

So the whole-task scan-6 path is exactly the scan-2 path minus three
instructions, in both shapes. The v0689 naive attempt (body 18, no other
change) overshot by `+2` because the recovered shell applies a measured
`-2` accounting correction
(`vf2_hybrid_coli_23524_execute`, the `scan_821 == 2` witness gate) that the
naive attempt left keyed on scan 2 only. Since the trace proves the shell
region is instruction-identical between scans 2 and 6, the same measured
correction covers scan 6; no `hybrid_execute_coli_body` `g6 == 4` change is
required (its bilateral `+2` fires identically for scans 2 and 6 and cancels
in the delta).

## F1 scan-6 single (9389)

The F1 single-live sibling was measured fresh here. Trace diff against the
committed F1 scan-5 single witness shows the streams are identical except
the first `0x22298` call, where scan 6 takes `0x2232c -> 0x2233c` directly,
skipping the two tests scan 5 walks (`cmpibe 5`, `bbs 12`): children span
`20` vs `22`, whole-task delta `-2`. Both take the same `0xffff`
ordering-fail tail. The reference child span is the same 20 measured for the
F0 scan-6 second call, so the ordering-fail body 18 covers both, and the
measured F1 scan-5 shell `-1` correction (whose shell stream is likewise
trace-proven identical between F1 scans 5 and 6) is extended to scan 6.

## Recovery

- `coli_22298_body` (`src/recovered/hybrid.c`): the measured ordering-fail
  sibling now admits `scan_821 == 6`, storing `0xffff` at `g7+0x6dc` with
  body 18 (scan 2's 21 minus the three skipped tests; identical span in the
  F0 second call and the F1 first call).
- `vf2_hybrid_coli_23524_execute`: the measured scan-2 `-2` correction gate
  now reads `scan_821 == 2 || scan_821 == 6`; the measured F1 scan-5 `-1`
  g3-scan correction gate now reads `scan_821_other == 5 || 6`.
- `vf2_hybrid_first_dispatch_task_execute`: the whole-task whitelist admits
  `9389/17/18` (single live, either fighter) and `9524/18/19` (F0 scan 6
  bilateral).

## Proof

`tests/recovered/test_coli_whole_task_live.c` drives the three recovered
measured shapes through the ROM-backed differential
(`vf2_coli_whole_task_live_differential`): the reference re-measures
`9389/17/18` (F0 single), `9524/18/19` (F0 bilateral) and `9389/17/18`
(F1 single) on every run and the native path matches counts and complete
CPU/condition/procedure/Model 2A live state at `0x10dcc`. A ROM-independent
unit pin in `test_native_runtime.c` fixes the new ordering-fail scan-6 child
body at 18 (+1 ret).

Everything outside the measured shapes remains fail-closed: unmeasured scan
values, non-zero `+0x61c`, non-failing orderings on unmeasured flag
compositions, the measured-but-undecomposed bilateral 6/6 shape (`9528`) and
other unmeasured bilateral scan pairs are unchanged or explicitly rejected.
