# `fa_player` `0x19ef8` measured quadruple `0x17` (v0567)

The live `0x00510980` / selector `0x505` corridor was measured with
`fighter+0x1a4` non-branch bits `0,1,2,4` set, producing non-branch mask
`0x00000017`. All sixteen combinations of branch bits `5,6,21,23` were
replayed from the existing `pre14288` park.

Every case reaches `0x1428c` with exact CPU, condition, local-frame,
procedure and mutable Model 2A state equality. Counts are `1622` with no
branch bits and the measured branch corrections produce the exact values
through `1650`; every case has four calls and four returns. The native guard
admits only this measured mask in addition to the previous `0x0f` family.

The control mask `0x17 + bit7` remains unsupported, so the result does not
generalize to arbitrary four-bit combinations.
