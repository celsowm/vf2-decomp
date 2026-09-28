# fa_rob 0x14640 equality tails for states 27/28 (v0431)

At `0x14640`, with `+0x198 == 0`, `+0x654 != 0`, and signed
`+0x1aa == +0x62a`, the ROM takes the `0x14658` equality branch before the
state-specific dispatch. State 27 and state 28 therefore use the same
`0x146dc` tail already recovered for the neutral equality witness:

- store `+0x654` to `+0x194`;
- clear `+0x654`; and
- leave the helper at `0x146e8` after 10 body instructions.

The generic helper dispatcher consumes the `0x146e8` return, adding one
instruction and one procedure return. The state-27 and state-28 witnesses
both match at 11 total instructions / one return, including registers,
condition state, frames and Work-RAM.

The unequal state-27 type-15 walk, unequal state-28 arithmetic tail, board
bit-20 shifts and other unmeasured compositions remain fail-closed.

## Pin

`vf2_player_14640_compare_escape_live` runs the neutral direct equality case
plus generic-dispatch state-27 and state-28 equality cases.
