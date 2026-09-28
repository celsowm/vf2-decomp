# `fa_player` `0x19ef8` measured five-bit low mask (v0569)

The live `0x00510980` / selector `0x505` corridor was replayed from the
`pre14288` park with `fighter+0x1a4` non-branch mask `0x0000001f` (bits 0..4).
All sixteen combinations of branch bits `5,6,21,23` match the reference at
`0x1428c` with exact CPU, condition state, local frame, procedure state and
mutable Model 2A memory. Counts are `1622` with no branch bits and the exact
measured corrections through `1650`; every case has four calls and four
returns.

At v0569 the control `0x0000009f`, which adds non-branch bit 7, remained
unsupported. It was measured and admitted in the subsequent v0570 slice; the
then-unmeasured bit-8 extension remains outside this note's evidence.
