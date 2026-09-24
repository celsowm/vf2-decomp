# v0416: recover the `0x14570` state-16 joins

## Verdict

The measured `0x14528` branch joins into the type-5 tail at `0x14570` for
three state-16 families:

- `(r7,r8)=(16,0)`: direct tail, 52 instructions to `0x1463c`;
- `(r7,r8)=(0,16)`: the `0x14564..0x1456c` g7/g8 swap, 55 instructions;
- `(r7,r8)=(16,16)`: `0x500028` bit 0 clear takes the direct 54-instruction
  path, while bit 0 set takes the swapped 58-instruction path.

All four joins use the measured short tail: a valid type-5 record at
`+0x194(g7)`, bit 0 of `+0x1a4(g8)` clear, bit 6 of the
`0x50016c+0x3351` byte clear and bit 9 of `0x508000` set. Each has one
`0x1ab34` call and return. The recovery keeps other state combinations,
scaling branches, text submission and walker misses unsupported.

## Pins

`vf2_player_1453c_live` runs six ROM-backed cases from
`out/park-1442c.vf2snap`: the two state-27 direct/swap shapes from v0415 and
the four state-16 joins above. Every case requires the measured instruction
count, call/return counters and full live-state equality.

The integrated `vf2_player_1442c_state16_live` fixture also drives both
fighters through the preceding `0x14640` neutral helpers and the real body
dispatch. It proves 100 instructions for the direct board gate and 104 for
the swapped gate, with three calls/returns and full live-state equality.
