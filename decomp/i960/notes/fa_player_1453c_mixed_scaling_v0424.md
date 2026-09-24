# v0424: recover mixed first/later scaling

## Verdict

The first `+0x1a4(g8)` bit-0 scaling and the later `+0x3351` bit-6 gate are
now native together for the measured state-27/state-16 shapes. When target
g8 bit 29 is clear, the path takes the second gate to the common store; when
it is set, it also takes the `0x145cc..0x145d4` scaling arm and selects
`0x1b982`.

Measured instruction counts to `0x1463c` are:

| Shape | Bit 29 clear | Bit 29 set |
| --- | ---: | ---: |
| state 27 direct | 57 | 60 |
| state 27 swapped | 61 | 64 |
| state 16 swapped | 60 | 63 |
| both state 16 direct | 59 | 62 |
| both state 16 swapped | 63 | 66 |

Each shape has one type-5 call/return. Scaled text compositions remain
fail-closed.

## Pin

`vf2_player_1453c_live` adds all ten cases to its matrix and requires exact
instruction count, call/return counters and full live-state equality.
