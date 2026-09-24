# `fa_player` `0x14640`: bounded type-15 walker miss sweep v0476

## Evidence

The reference executor was driven from `out/park-1442c.vf2snap` at
`0x00014640` with fighter `g7 = 0x00510980`, `+0x198 = 0`,
`+0x654 = 0`, and `+0x197 = 27`. The low-half selector at `g7 + 0x194`
was swept through `1..32`, with bit 20 of `0x00500068` clear, and execution
was stopped at `0x000146c4`.

All 32 selectors miss the type-15 record walk and still reach the common
return through the measured zero-record tail. The reference instruction
counts range from 36 to 51 depending on the walk length; every case has one
nested call and return.

## Recovery boundary

The native state-27 `0x14640` body now admits exactly the bounded miss sweep
`1..32`. For a miss, the recovery preserves the measured zero-record value
used by the following `ldos`/subtract sequence, then performs the existing
`+0x62a`, `+0x654`, and `+0x194` stores. Other miss selectors and the
unmeasured board/state siblings remain `VF2_ERROR_UNSUPPORTED`.

## Acceptance

`tests/recovered/test_player_14640_state27_live.c` compares all 32 selectors
against the reference for exact instruction counts, registers, condition
state, frames, call/return counters, and mutable Model 2A memory.
