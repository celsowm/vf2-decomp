# `fa_player` `0x19ef8` measured high-mask extensions (v0580)

The live `0x00510980` / selector `0x505` corridor was replayed from the
`pre14288` park for the measured masks `0x0200059f`, `0x0400059f`,
`0x0800059f`, `0x1000059f`, `0x4000059f` and `0x8000059f`. The reference
reaches `0x1428c` for each mask; the focused differential fixture covers all
16 combinations of branch bits 5, 6, 21 and 23 for each and matches CPU,
condition state, local frame, procedure state and mutable Model 2A memory.

The native guard admits only these measured masks. The branch-bit alias
`0x2000059f` is already covered by the existing bit-21 matrix and was not
added as a duplicate mask. Other larger combinations remain fail-closed.
