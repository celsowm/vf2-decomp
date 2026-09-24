# `fa_player` `0x14640`: extended type-15 selector sweep v0478

The v0477 state-27 evidence was extended through selector `0x80` using the
same parked machine state and stop boundary:

```text
g7 = 0x00510980, +0x198 = 0, +0x654 = 0, +0x197 = 27,
bit 20 of 0x00500068 clear, stop at 0x000146c4
```

Every selector `65..128` reaches `0x146c4`. Selector `115` returns a type-15
record through the existing hit path; the other 63 selectors are measured
walker misses. Their reference instruction counts range from 29 to 72, with
one nested call and return in each case.

The live fixture now compares the complete selector range `1..128` against
native execution for exact instruction counts, registers, condition state,
frames, call/return counters, and mutable Model 2A memory. The recovery admits
only measured misses in that range; other type-15 misses remain
`VF2_ERROR_UNSUPPORTED`.
