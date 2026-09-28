# fa_rob 0x1453c/0x14570 complete text matrix (v0430)

The board-bit-9-clear text branch at 0x145ec is now measured for all 35
state/scaling shapes already admitted by the 0x1453c recovery:

- direct and swapped state 27;
- direct, swapped and both-state-16 joins;
- first-scaling, second-gate and later-scaling variants; and
- mixed first/second and first/later scaling variants.

The recovered 0x7fc0 byte expander adds 74 instructions and one call/return
to each corresponding non-text path. The final state matches the original
including registers, condition state, procedure counters, frames and
Work-RAM.

The type-5 walker miss and unrecognized state/scaling shapes remain
fail-closed.

## Pin

vf2_player_1453c_live runs the original 35 non-text cases plus the 34
additional text cases (the direct state-16 text baseline is already part of
the original matrix), with exact instruction/call/return counts and full
live-state equality.
