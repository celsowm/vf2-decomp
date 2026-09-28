# `fa_player` bounded direct state-16 family (v0443)

At `0x14528`, the direct `r7 == 16` arm reaches `0x14570` whenever the
following `r8` comparison is not the dedicated state-16 or state-27 join. A
controlled reference sweep from `out/park-1442c.vf2snap` covered `r8=0..31`
with valid type-5 index `0x73` at the selected fighter's `+0x194`.

| `r8` domain | reference path |
| --- | --- |
| `0..31`, excluding `16` and `27` | 52 instructions, `+1/+1` call/return |
| `16` | dedicated 54-instruction both-state-16 path |
| `27` | dedicated 56-instruction swapped state-27 path |

Native C now admits only the measured `r8=0..31` direct family. The focused
ROM fixture proves the state-26 witness at 52 instructions and its
board-bit-9-clear text variant at 126 instructions, with exact CPU,
condition-state, frame, and Work-RAM equality. Values above 31 and other
unmeasured state/scaling combinations remain fail-closed.
