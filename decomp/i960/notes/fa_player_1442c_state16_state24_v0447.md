# `fa_player` integrated state-16/state-24 neutral continuation (v0447)

The `0x1442c` row with fighter 0 state 16 and fighter 1 state 24 swaps into
the `0x14474` arm. With fighter 0 `+0x19f == 0`, the two `cmpobe/cmpobne`
tests miss, the `0x14498` restore runs, and `0x144a0/0x144a4` fall through to
`0x14528` with the direct state-16 registers.

The reference reaches `0x1463c` in 105 instructions with three calls and
three returns from `out/park-1442c.vf2snap`, using type-5 index `0x73` on
fighter 0. Native C now matches the complete CPU, condition-state, frame, and
Work-RAM state. The state-24 `+0x19f` values 25/22 write arms remain separate
boundaries, as do unmeasured flag and scaling compositions.
