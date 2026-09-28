# fa_rob `0x144b0` state-25 to state-16 successor (v0433)

The measured `0x144b0` state-25 arm reaches the shared `0x14570` body when
the second fighter has state byte 16 and the `0x1450c cmpobl` takes the
`r13 < r3` arm.  The witness uses fighter0 state 25 with zero low-half
`+0x194`, fighter1 state 16 with type-5 index `+0x194 = 0x73`,
`+0x808(f1) = 2`, `+0x858(f0) = 0`, and `+0x1aa(f1) = 0`.

After the `0x14518` `+0x654`/`+0x62a` stores, the ROM takes the
`0x14560` state-16 fall-through, swaps g7/g8 at `0x14564..0x1456c`, and
enters the already recovered type-5 `0x14570` body.  The direct arm reaches
`0x1463c` in 99 instructions with two calls and two returns, including the
`0x19ef8` and type-5 `0x1ab34` calls.  The integrated `0x1442c` witness
reaches the same boundary in 144 instructions with four calls and four
returns; both reference/native states compare exactly.

The native recovery admits only this measured `r7 = 25, r8 = 16` join and
retains the existing fail-closed guards for other state-25 successors.
The focused fixture is `vf2_player_1442c_live` and covers both direct and
integrated forms.
