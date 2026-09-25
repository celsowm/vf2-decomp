# `fa_player` `0x19ef8` measured mask `0x100059f` (v0579)

The live `0x00510980` / selector `0x505` corridor was replayed from the
`pre14288` park with `fighter+0x1a4` mask `0x0100059f`, extending the
previous measured `0x0000059f` shape with bit 24. All sixteen combinations
of branch bits 5, 6, 21 and 23 reach `0x1428c` and match the native recovery
with exact CPU, condition state, local frame, procedure state and mutable
Model 2A memory. The reference corridor remains 1622 instructions with the
measured branch corrections through 1650.

The native guard now admits this measured mask. No generalization to
arbitrary larger combinations is implied.
