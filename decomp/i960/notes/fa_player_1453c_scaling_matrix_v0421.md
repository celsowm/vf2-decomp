# v0421: extend the first-scaling matrix

## Verdict

The first `+0x1a4(g8)` bit-0 scaling arm at `0x145a0..0x145a8` is now
native for all measured state shapes. The type-5 record byte is scaled by
`r3 + (r3 >> 2)` and the table is selected as `0x1b979`.

Measured instruction counts to `0x1463c` are:

| Shape | Steps |
| --- | ---: |
| state 27 direct | 55 |
| state 27 swapped | 59 |
| state 16 swapped | 58 |
| both state 16 direct | 57 |
| both state 16 swapped | 61 |

Each shape has one type-5 call/return. The recovery admits these exact
state-shape gates and keeps unmeasured scaling/text compositions fail-closed.

## Pin

`vf2_player_1453c_live` adds all five cases to its matrix and requires exact
instruction count, call/return counters and full live-state equality.
