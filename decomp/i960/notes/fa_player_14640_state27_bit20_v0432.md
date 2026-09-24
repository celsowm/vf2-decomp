# fa_rob 0x14640 state-27 board-bit-20 shift (v0432)

The measured state-27 type-15 walk with `0x500068` bit 20 set takes the
`0x14684 shli 1, r4, r4` arm before the existing `subo 1, r4, r4` and
`+0x62a` store. Relative to the unequal compare-prefix witness, it adds one
instruction and otherwise preserves the same type-15 call/return accounting.

The recovered path reaches `0x146c4` in 45 instructions with one call and one
return. The focused state-27 fixture compares the ROM and native CPU/machine
states byte-for-byte, including the shifted `+0x62a` result.

Other board-shift compositions remain fail-closed.
