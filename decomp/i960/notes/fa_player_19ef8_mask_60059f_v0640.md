# v0640 — `fa_player` `0x19ef8` mask `0x00060059f`

The live selector-`0x505` corridor was measured with non-branch state mask
`fighter+0x1a4 = 0x00060059f` (the existing low mask plus bits 17 and 18),
across all 16 subsets of branch bits 5, 6, 21 and 23. Every case reaches
`0x1428c` with full CPU, condition, local-frame, procedure-state and mutable
Model-2A equality. Instruction deltas are `1634, 1643, 1639, 1648` and the
matching branch variants, with four calls and four returns.

The neighboring masks formed by adding other unmeasured high bits remain
explicit unsupported controls.
