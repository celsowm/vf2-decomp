# `fa_player` `0x19ef8` measured quadruple `0x0f` (v0566)

The live `0x00510980` / selector `0x505` corridor was measured with
`fighter+0x1a4` non-branch bits `0,1,2,3` set, producing non-branch mask
`0x0000000f`. All sixteen combinations of branch bits `5,6,21,23` were
replayed from the existing `pre14288` park.

Every case reaches `0x1428c` with exact CPU, condition, local-frame,
procedure and mutable Model 2A state equality. Counts are `1622` with no
branch bits and the measured branch corrections produce the exact values
through `1650`; every case has four calls and four returns.

The native guard now admits only non-branch mask `0x0000000f` in addition to
the previously proven families of at most three non-branch bits. Other
quadruple masks and five-or-more non-branch bits remain fail-closed.
