# fa_rob `0x1442c` state-25/state-27 neutral continuation (v0436)

The measured integrated setup enters `0x1442c` with fighter0 state 25 and
fighter1 state 27.  The live `0x14640` state-27 helper stores its measured
state-27 result and clears fighter1 `+0x194`; the later `+0x197` read therefore
sees the neutral state and takes the existing state-25 `0x144b0` arm.

The ROM-backed fixture proves exact live-state equality at `0x1463c`: 126
instructions with four calls/returns.  The nine-instruction `0x144a0` to
`0x144b0` prefix is distinct from the previously measured state-24 prefix.
Direct entry at `0x144b0` with fighter1 state 27 remains fail-closed because
that is not the measured integrated continuation.
