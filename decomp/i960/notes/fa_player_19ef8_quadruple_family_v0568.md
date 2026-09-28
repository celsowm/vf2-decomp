# `fa_player` `0x19ef8` measured quadruple family (v0568)

The live `0x00510980` / selector `0x505` corridor was replayed from the
`pre14288` park for non-branch masks `0x0000001b`, `0x0000001d` and
`0x0000001e`. These are respectively bits `0,1,3,4`, `0,2,3,4` and
`1,2,3,4` at `fighter+0x1a4`.

For each mask, all sixteen combinations of branch bits `5,6,21,23` match the
reference at `0x1428c`, including CPU, condition state, local frame,
procedure state and mutable Model 2A memory. Counts are `1622` with no branch
bits and the exact measured corrections through `1650`; every case has four
calls and four returns.

Each fixture also runs a bit-7 extension as an unsupported control. The guard
therefore admits the measured family `0x0f`, `0x17`, `0x1b`, `0x1d`, `0x1e`,
not arbitrary four-bit masks.
