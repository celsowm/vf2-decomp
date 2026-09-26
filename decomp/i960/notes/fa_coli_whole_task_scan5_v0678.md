# `fa_coli` whole-task single-live scan-5 sibling

## Boundary

The fixture starts from the measured parked `0x221e8` checkpoint and dispatches
the complete `fa_coli` task to `0x10dcc`. It compares the reference executor
with `vf2_hybrid_first_dispatch_task_execute`, including CPU state, condition
state, local frames, counters and mutable Model 2A memory.

## Measured mutation

- fighter 0 `+0x1a4 = 0x00000100`;
- fighter 0 `+0x820 = 0`, `+0x821 = 5`;
- fighter 1 `+0x1a4` bit 8 remains clear;
- the task pending word is `0xffff`.

The second `0x22298` invocation reads fighter 1 as `g7` and fighter 0 as
`g8`. With `g7 + 0x61c == 0`, `g8 + 0x821 == 5`, and the measured ordering
`g7 + 0x1f8 >= g7 + 0x6e4`, the ROM stores `0xffff` at `g7 + 0x6dc` and
consumes the 20-instruction body. The following contact path reaches the
zero-contact exit.

## Differential result

The reference and native runs both finish at `0x10dcc` with
`9391/17/18` instruction/call/return deltas and full live-state equality.
The test also retains the four previously accepted whole-task shapes. The
opposite ordering and unmeasured neighboring scan combinations remain
`VF2_ERROR_UNSUPPORTED`.
