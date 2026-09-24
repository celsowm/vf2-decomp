# v0422: extend the `0x145c0` second-gate matrix

## Verdict

The `+0x3351` bit-6 second gate at `0x145c0` is now native for the measured
state-27/state-16 shapes when the g8 word bit 29 is clear. The ROM takes the
`0x145c4` load and the bit-29 branch to the common `0x145dc` store; this adds
two instructions over each corresponding short path.

Measured instruction counts to `0x1463c` are:

| Shape | Steps |
| --- | ---: |
| state 27 direct | 54 |
| state 27 swapped | 58 |
| state 16 swapped | 57 |
| both state 16 direct | 56 |
| both state 16 swapped | 60 |

Each shape has one type-5 call/return. The first-scaling bit-0 gate remains
required to be clear for this matrix; bit-29-set and mixed scaling variants
remain fail-closed.

## Pin

`vf2_player_1453c_live` adds all five cases to its matrix and requires exact
instruction count, call/return counters and full live-state equality.
