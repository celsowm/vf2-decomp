# `fa_rob` `0x14640` state-27 type-15 miss sweep v0489

## Evidence

The ROM-backed state-27 fixture was driven from `out/park-1442c.vf2snap`
with fighter `+0x197 = 27`, a clear board bit 20, and each selector
`+0x194 = 129..256`. Every reference walk returned `g0 == 0`, proving a
type-15 miss. The measured return spans were 29 through 72 instructions,
with one nested call and return per case.

## Recovery boundary

The existing measured miss tail is extended from the exact selector range
`1..128` to `1..256`. Type-15 hits continue through the generic table walker;
misses outside the measured range remain `VF2_ERROR_UNSUPPORTED`.

## Validation

`tests/recovered/test_player_14640_state27_live.c` compares all 128 new
reference/native cases for exact instruction count, registers, condition and
procedure state, call/return counts, and mutable Model 2A memory.
