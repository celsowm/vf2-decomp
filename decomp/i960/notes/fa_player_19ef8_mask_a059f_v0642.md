# v0642 — `fa_player` `0x19ef8` mask `0x000a059f`

The live selector-`0x505` corridor was measured with non-branch state mask
`fighter+0x1a4 = 0x000a059f` (the existing low mask plus bits 17 and 19),
across all 16 subsets of branch bits 5, 6, 21 and 23. Every case reaches
`0x1428c` with full CPU, condition, local-frame, procedure-state and mutable
Model-2A equality. Instruction deltas are `1622..1650` according to the
branch subset, with four calls and four returns.

Other unmeasured high-bit combinations remain explicit unsupported controls.
