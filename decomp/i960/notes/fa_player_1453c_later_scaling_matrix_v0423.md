# v0423: extend the later-scaling matrix

## Verdict

The bit-29 later-scaling arm at `0x145cc..0x145d4` is now native for the
measured state-27/state-16 shapes when the `+0x3351` byte bit 6 is set and
the target g8 word bit 29 is set. It performs the second `shro`/`addo` and
selects table `0x1b982`.

Measured instruction counts to `0x1463c` are:

| Shape | Steps |
| --- | ---: |
| state 27 direct | 57 |
| state 27 swapped | 61 |
| state 16 swapped | 60 |
| both state 16 direct | 59 |
| both state 16 swapped | 63 |

Each shape has one type-5 call/return. The first-scaling bit-0 gate is clear
for this matrix; mixed first/later scaling and scaled text variants remain
fail-closed.

## Pin

`vf2_player_1453c_live` adds all five cases to its matrix and requires exact
instruction count, call/return counters and full live-state equality.
