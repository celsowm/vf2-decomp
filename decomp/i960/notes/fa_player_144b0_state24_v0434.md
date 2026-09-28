# fa_rob `0x144b0` state-25 to state-24 successor (v0434)

The measured state-25 arm reaches the common `0x14628` exit when fighter1
has state byte 24 and the `0x1450c cmpobl` takes (`r13 < r3`).  The
preceding `0x1442c` branch takes the measured `0x14474`/`0x14498` escape for
the neutral fighter0 `+0x19f` value, restoring g7/g8 before entering
`0x144b0`.

The direct `0x144b0` witness reaches `0x1463c` in 53 instructions with one
call/return, matching the existing state-25 common-exit body.  The integrated
`0x1442c` witness reaches the same boundary in 105 instructions with three
calls/returns.  Both forms compare equal across CPU state, condition state,
frames and mutable Model 2A memory.

Only the measured `+0x19f` miss of both downstream values is admitted; the
other state-24 `0x14474` compositions remain fail-closed.
