# `fa_player` bounded neutral `0x14528` family (v0442)

The `0x14528` dispatch reaches the common exit when `r8` is not 27, `r7` is
not 16, and `r8` is not 16. A controlled reference sweep from
`out/park-1442c.vf2snap` measured the bounded `r8 == 0`, `r7=0..31` domain.

| `r7` domain | reference path |
| --- | --- |
| `0..31`, excluding `16` and `27` | 9 instructions, `+0/+0` call/return |
| `16` | dedicated 52-instruction type-5 path |
| `27` | dedicated 52-instruction state-27 path |

The 9-instruction path is the three dispatch comparisons followed by the five
instructions at `0x14628..0x14638`. It clears both fighters' `+0x198` fields
and leaves the final compare as GREATER for the measured `r8 == 0` shape. The
native recovery admits only the measured `r7=0..31` family and the focused
fixture proves the state-26 witness with exact CPU, condition-state, frame, and
Work-RAM equality. Values above 31 and other `r8` combinations remain
fail-closed.
