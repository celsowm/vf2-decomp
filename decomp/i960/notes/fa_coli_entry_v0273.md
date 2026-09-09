# fa_coli recurring entry via PUNCH-driven warm boot — v0273

## Summary

The v0269 hunt proved `fa_coli` recurring body `0x221e8` needs
`entry=0x221e8 + runnable` at a scanning sweep, reachable "only in gameplay
frames via driven inputs". That conjunction is now reproduced: holding PUNCH
(`--input 16`) from the sixth-dispatch snapshot under strict per-block
differential runs the phase-11 countdown (`0x00500024`) to its terminal,
clears the phase flag (`0x8b -> 0x0b`), and arms slot 10 with
`entry=0x221e8, flags=0x80000000`. The next scheduler sweep dispatches it.

The scan prefix through the `callx` dispatch (187 instructions, 4 calls,
2 returns) is now native and exact. The `fa_coli` body at `0x221e8` remains
an explicit boundary.

No ROM, snapshot, trace, or other proprietary artifact is stored here. All
measurements reproduce with a locally supplied supported ROM set; snapshots
below live in scratch space, not in the repo.

## Drive chain (all strict per-block MATCH unless noted)

From `sixth.vf2snap` (`native-sixth-dispatch`, MATCH, 8,675,721 insns):

- `vf2cycles --input 16 --cycles 10` MATCH; sampled word `0x0f000100`
  (PUNCH), phase `0x8b`, countdown `0x138` (312, already decrementing).
- `+100 cycles` MATCH; countdown `0xde` (222).
- `+150 cycles` MATCH; countdown `0x48` (72). Rate is 1/frame throughout.
- `+100 cycles` stops `71/100`: native frame-wait `0x0f7c -> 0x0bc0`
  against reference parked at `0x0f7c` (see oracle fix below).
- After the oracle fix, same start reaches `73` cycles / 3,095 blocks /
  7,015,956 instructions MATCH, then stops at the scheduler sweep below.

Countdown arming to 320 was not re-observed (it arms between sixth and the
first read); the measured span 639 (reference-parked sixth) -> 312 -> 222
-> 72 -> 0 at 1/frame matches the v0270 320-frame terminal shape.

## Warm pre-step state (measured, `0x0a010` scheduler front-end)

- `0x00500024` (countdown) = 0 (terminal).
- `0x005000a4` phase index = `0x0b` (flag cleared from `0x8b`).
- `0x00500068` ready flags = `0x80004400` (bit 16 clear -> full 29-task scan).
- `0x00508000` runtime flags = `0x8a00` (bit 9 set -> recurring scan path,
  bit 5 clear).
- task count `0x11d94` = 29; timers `0xf00004/0xf00008` = `0xfffff`.
- Registry: slots 0-9 inactive (no bit 31), slot 10 `flags=0x80000000`,
  `entry=0x000221e8` — first runnable descriptor.

## Reference scan (`vf2probe --until 0x000221e8 --trace --memory-trace)

187 instructions, 4 calls, 2 returns, `0xa010 -> 0x10d54 -> 0x221e8`:

- `call 0xa010 -> 0x10d54`, two `0x7b18` geometry helpers (status 0,
  command 3, status 0, command 1), ready-flags prologue: 13 + 7 fixed.
- Per scanned descriptor (16 instructions): index store `0x500038`,
  timer reload `0xf00004 = 0xfffff`, flags load, `ldl` timer, elapsed
  computation (`and`/`subi`, always 0 against the static reloaded timer),
  elapsed store to scratch `0x50c000 + 0x10 + i*0x20`, stride load,
  registry/scratch/index advance, count load, `cmpibl` loop.
- Runtime bit 9 set skips the `0x10da0` input-pointer check block.
- Selected tail (7): index/timer/flags stores and loads, `bbc` not taken
  (bit 31 set, compare leaves EQUAL), `ld` entry, `callx -> 0x221e8`.
- Total: `27 + 16 * index` (235 for index-13 game_info, 187 for index-10
  coli). Stride walk verified per descriptor against the trace reads.
- r0 is never written on this path (destinations are
  g0/g13/r3/r4/r8-r11/r13-r15 plus calls); it is caller-carried
  (`0x005ff500` cold corridor, `0x005ff640` warm).

## Code changes

- `hybrid.c`: admit `VF2_TASK_COLI_ENTRY (0x000221e8)` in
  `hybrid_second_scheduler_task_supported`; scan accounting `27 + 16*index`
  (was flat 235); preserve caller r0 instead of forcing `0x005ff500`
  (r2/r16 forces retained — measured equal in both histories).
- `native_differential.c`: `run_until_after` and `probe_cycles` handle a
  zero-instruction FRAME_WAIT step by observing the interrupt on the
  reference side (mirrors `vf2_native_differential_step`), instead of
  failing closed. Non-FRAME_WAIT zero-count steps still fail closed.
- `native_differential_step.c`: zero-initialize `frame_wait_before`
  (MSVC C4701 warning-as-error broke the build on master).
- `tests/recovered/test_native_runtime.c`:
  `test_scheduler_selects_coli_entry_at_index10` (synthetic 10-inactive +
  coli registry: 187/4/2, index 10, entry ip, plus fail-closed next step
  at the unrecovered body).

## Proof

- Strict per-block from the warm checkpoint: scheduler step exact
  (187/187 instructions, 4/2 calls/returns, both sides at `0x221e8`,
  full CPU/condition/frame/counter/RAM equality), then stops at the first
  `fa_coli` body block — the new explicit boundary.
- Full `ctest -C Debug` green (56/56) with the generalized accounting and
  the r0 preservation, so the cold corridor is unchanged.

## Remaining frontier

`fa_coli` body at `0x221e8`: `ld 0x508000`, `bbs 5 -> 0x22294` gate, then
the `0x23524` / `0x22298` call sequence (the v0268 callee family). The
entry checkpoint shape (slot-10 selection) is the fixture to extend from.
