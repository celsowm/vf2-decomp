# `fa_player` integrated state-16/state-27 join (v0446)

The live `0x1442c` state row with fighter 0 `+0x197 == 16` and fighter 1
`+0x197 == 27` first runs the state-27 helper. That helper changes the
post-helper state byte to 16, so the later `0x14528` dispatch uses the already
recovered both-state-16 join.

From `out/park-1442c.vf2snap`, with valid type-5 index `0x73`, the reference
reaches `0x1463c` in 126 instructions with four calls and four returns. The
expanded state-16 live fixture now compares the native path byte-for-byte,
including CPU state, condition state, frames, and Work RAM. The state-24
special continuation remains unimplemented and fail-closed.
