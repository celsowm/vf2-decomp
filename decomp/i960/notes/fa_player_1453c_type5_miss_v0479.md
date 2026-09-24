# `fa_player` `0x1453c/0x14570`: extended type-5 selector sweep v0479

The v0475 direct state-27 evidence was extended through selector `0x80` from
the same `out/park-1442c.vf2snap` setup, stopping at `0x0001463c`.

Every selector `33..128` reaches the common return. The range contains both
type-5 record hits and zero-record walker misses; the measured instruction
counts range from 47 to 90, with one nested call and return for the short
tail. The reference/native fixture compares all 96 added cases for exact
instruction counts, registers, condition state, frames, call/return counters,
and mutable Model 2A memory.

The native boundary admits measured type-5 misses only within the fully swept
selector range `1..128`, while preserving the earlier `0x0110` and `0x02cf`
witnesses. Other misses and unrecognized state/scaling shapes remain
`VF2_ERROR_UNSUPPORTED`.
