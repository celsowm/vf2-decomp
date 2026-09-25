# v0639 — `fa_player` `0x19ef8` mask `0x00020059f`

The live selector-`0x505` corridor was measured with non-branch state mask
`fighter+0x1a4 = 0x00020059f` (the existing low mask plus bit 17), across all
16 subsets of branch bits 5, 6, 21 and 23. Every case reaches `0x1428c` with
full CPU, condition, local-frame, procedure-state and mutable Model-2A
equality. Instruction deltas are `1634, 1643, 1639, 1648` and the matching
branch variants, with four calls and four returns.

The neighboring mask formed by adding bit 16 remains an explicit unsupported
control. No other larger mask was admitted by this change.
