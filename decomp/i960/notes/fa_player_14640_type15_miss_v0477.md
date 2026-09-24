# `fa_player` `0x14640`: extended type-15 walker miss sweep v0477

The v0476 state-27 evidence was extended from selector `1..32` through
selector `1..64` using the same parked machine state:

```text
g7 = 0x00510980, +0x198 = 0, +0x654 = 0, +0x197 = 27,
bit 20 of 0x00500068 clear, stop at 0x000146c4
```

All selectors `33..64` also miss the type-15 record walk and reach the common
return through the zero-record arithmetic tail. Their reference instruction
counts range from 29 to 58, and each has one nested call and return.

`tests/recovered/test_player_14640_state27_live.c` now compares all 64
selectors against native execution for exact instruction counts, registers,
condition state, frames, call/return counters, and mutable Model 2A memory.
The native boundary admits only the measured `1..64` range; other type-15
misses remain `VF2_ERROR_UNSUPPORTED`.
