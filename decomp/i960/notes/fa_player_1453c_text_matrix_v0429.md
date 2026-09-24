# fa_rob 0x1453c/0x14570 text matrix (v0429)

The board-bit-9-clear branch at 0x145ec now has measured native coverage
through the recovered 0x7fc0 byte expander for:

- unscaled state-27 direct and swapped joins: 126 and 130 instructions;
- unscaled state-16 swapped and both-state-16 direct/swapped joins: 129, 128
  and 132 instructions; and
- direct state-16 first-scaling, second-gate and later-scaling variants: 129,
  128 and 131 instructions.

All shapes make two calls and two returns, including the type-5 walk and text
expander. The direct unscaled state-16 witness from v0420 remains the 126-step
baseline. Mixed first/later scaling and other unmeasured text compositions
remain fail-closed.

## Pin

vf2_player_1453c_live runs the eight new shapes alongside the existing matrix
and requires exact instruction/call/return counts and full
register/condition/frame/Work-RAM equality.
