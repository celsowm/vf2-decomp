# `fa_player` bounded state-16 swapped family (v0441)

The `0x14528` dispatch has a shared fall-through for `r8 == 16` after the
dedicated `r7 == 27` and `r7 == 16` tests. A controlled reference sweep from
`out/park-1442c.vf2snap` covered every bounded state-byte value `r7=0..31`
with a valid type-5 index `0x73` at the selected fighter's `+0x194`.

Measured short-path results:

| `r7` domain | reference path |
| --- | --- |
| `0..31`, excluding `16` and `27` | 55 instructions, `+1/+1` call/return |
| `16` | dedicated 54-instruction both-state-16 path |
| `27` | dedicated 52-instruction state-27 path |

The native recovery now admits only the measured `0..31` family through the
existing `0x14564..0x1456c` swap. The focused ROM fixture proves the new
state-26 short and board-bit-9-clear text witnesses at 55 and 129
instructions, with exact CPU, condition-state, frame, and Work-RAM equality.
Values above 31, other scaling combinations, and walker misses remain
fail-closed.
