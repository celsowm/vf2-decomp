# `fa_player` integrated state-16/state-24 write arms (v0448)

The integrated `0x1442c` row with fighter 0 state 16 and fighter 1 state 24
reaches `0x14474` after the helper calls. For fighter 0 `+0x19f == 25` or
`22`, the ROM writes `0x01000000` to fighter 0 `+0x194`, clears bit 0 of its
`+0x1a4`, and rejoins the common exit.

From `out/park-1442c.vf2snap`, the reference paths reach `0x1463c` in 59 and
60 instructions respectively, with two calls and two returns. The native
recovery now matches both complete live states, including CPU, condition state,
frames, and Work RAM. Other state-24 write/flag/scaling compositions remain
fail-closed.
