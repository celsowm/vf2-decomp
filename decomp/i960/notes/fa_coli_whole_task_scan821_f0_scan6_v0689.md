# `fa_coli` whole-task `field_0821 = 6` — measured boundary (v0689)

## Verdict

The `field_0821 = 6` whole-task shape is **measured but still fail-closed**.
This note confirms the v0686 numbers independently, records the exact ROM
branch semantics, and documents the precise remaining native gap so the next
attempt does not have to re-derive it.

No C change is admitted here.

## Measured reference (reproduced independently)

Parked snapshot `out/coli-parked-221e8.vf2snap` (fighter0 `0x00510980`,
fighter1 `0x00512980`), stopping at `0x00010dcc`:

```sh
build/Debug/vf2probe.exe --rom-dir roms/vf2 \
  --snapshot out/coli-parked-221e8.vf2snap \
  --set-u32 0x00510b24=0x100 --set-u32 0x00510d84=0 \
  --set-u8 0x005111a1=6 --set-u16 0x005111a2=0 \
  --set-u32 0x00512b24=0   --set-u32 0x00512d84=0 \
  --set-u8 0x005131a1=0 --set-u16 0x005131a2=0 \
  --set-u32 0x005149cc=0xffff --until 0x00010dcc
```

| shape | fighter flags | f0 `+0x821` | f1 `+0x821` | reference |
| --- | --- | --- | --- | --- |
| single live | `0x100 / 0` | 6 | 0 | **9389 / 17 / 18** |
| both live | `0x100 / 0x100` | 6 | 0 | **9524 / 18 / 19** |

Counter deltas were taken against a zero-step baseline run
(`--until 0x000221e8`, which reports `procedure_calls=44717`,
`procedure_returns=44712`). The `run_instructions` field of the same run is
the instruction delta. Both shapes reproduce v0686 exactly.

Control: the same commands with `--set-u8 0x005111a1=2` give the committed
`9392/17/18` and `9527/18/19`, so the measurement recipe is aligned with the
test harness.

## ROM branch semantics (`0x2232c`)

```text
0x2232c  cmpibe 6, r6, 0x2233c     ; r6 == 6 jumps straight to the loop entry
0x22330  cmpibe 5, r6, 0x22338
0x22334  cmpibne 2, r6, 0x223b4
0x22338  bbs 12, r7, 0x223a8
0x2233c  ld 0x1f8(g7), r4
0x22340  ld 0x6e4(g7), r5
0x22344  cmpr r4, r5
0x22348  bge 0x223a8               ; r4 >= r5 -> 0xffff common tail
0x223a8  lda 0xffff, r11
0x223b0  b 0x223b4
0x223b4  stos r11, 0x6dc(g7)
0x223b8  ret
```

So for `r6 == 6` the ROM skips the three tests at `0x22330/0x22334/0x22338`
that scan 2 walks, reaches the same `cmpr/bge` ordering check, and on the
measured zero ordering (`g7+0x1f8 == g7+0x6e4 == 0`) takes the same `0x223a8`
`0xffff` tail. The `0x22298` child body is therefore **three instructions
shorter than scan 2** for that call. Only one of the two `0x22298` invocations
reads fighter 0's scan (the other call's `g8` is fighter 1, whose scan is 0),
which matches the observed whole-task delta of exactly `-3`.

## Remaining native gap

- `coli_22298_body` (`src/recovered/hybrid.c`) admits scans 2 and 5 in the
  ordering-fail branch but rejects 6 at the later
  `scan_821 == 2 || 5 || 6` guard, so the whole task returns
  `VF2_ERROR_UNSUPPORTED`.
- Adding `scan_821 == 6` to the ordering-fail branch with the derived body `18`
  is **not sufficient**: the recovered whole-task then reports `9391/17/18`
  (single) and `9526/18/19` (both), i.e. `+2` against the reference. The extra
  `+2` comes from the `g6 == (1U << 2)` adjustment in
  `hybrid_execute_coli_body`, which adds a measured `+2` for shapes whose
  fighter-0 scan differs from 5. That `+2` is not attributable to the scan-6
  path and must be re-measured before the shape can be admitted.
- The whole-task acceptance whitelist in
  `vf2_status vf2_hybrid_first_dispatch_task_execute` still does not list
  `9389/17/18` or `9524/18/19`; the ROM-backed matrix in
  `tests/recovered/test_coli_whole_task_live.c` does not drive scan 6.

Everything outside the measured shapes (other scan values, non-zero
`+0x61c`, non-failing ordering) remains fail-closed.
