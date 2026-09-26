# `fa_coli` whole-task both-live bit-15 sibling

## Boundary

The fixture starts from the measured parked `0x221e8` checkpoint and dispatches
the complete `fa_coli` task to `0x10dcc`. It compares the reference executor
with `vf2_hybrid_first_dispatch_task_execute`, including CPU state, condition
state, local frames, counters and mutable Model 2A memory.

## Measured mutation

- fighter 0 and fighter 1 `+0x1a4` both have bit 8 set;
- fighter 0 `+0x804 = 0x00008000`;
- fighter 0 `+0x820 = 0`, `+0x821 = 0`, `+0x822 = 0`;
- the task pending word is `0xffff`.

The first `0x22404` contact query takes the shorter measured scan path for the
index-zero field, while the second query follows the matching both-live shape.
Both contact results are zero and the midbody returns through the ordinary
zero-contact tail. The generic `0x238a4` shell also has the measured one-step
accounting reduction for this exact both-live field shape.

## Differential result

The reference and native runs both finish at `0x10dcc` with
`9520/18/19` instruction/call/return deltas and full live-state equality.
The test retains the previously accepted whole-task shapes, and neighboring
bit-15/order combinations remain `VF2_ERROR_UNSUPPORTED`.
