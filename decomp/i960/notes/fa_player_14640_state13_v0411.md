# v0411: recover the fa_rob state-13 neutral tail

The `0x14640` helper now admits the measured state-13 tail when
`+0x198 == 0`, `+0x654 == 0`, `+0x197 == 13` and bit 4 of `(g7)` is set.
The shared state checks reach `0x146b8`, take the state-13 branch to
`0x146c8`, load the nonzero `+0x194`, then clear `+0x654` and stop at the
unconsumed `0x146d8` return.  The complete span is 14 instructions with no
calls or returns; the final `cmpobe 0,r14` leaves LESS condition state.

`vf2_player_14640_state13_live` proves 14/14 instructions and full live-state
equality against the reference.  The `+0x197` values other than 13 and the
bit-4-clear sibling remain covered only where independently measured.
