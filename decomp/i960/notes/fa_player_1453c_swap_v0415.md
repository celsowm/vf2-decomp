# v0415: recover the `0x1453c` state-27 swap path

## Verdict

The measured second-fighter state-27 arm is now native. At `0x14528`, the
shape has `r7 != 27` and `r8 == 27`, so the ROM executes:

```text
0x14530 mov g8,r15
0x14534 mov g7,g8
0x14538 mov r15,g7
```

The existing type-5 tail then runs with the state-27 fighter as `g7`. The
measured short path reaches `0x1463c` in 56 instructions with one
`0x1ab34` call and return. It preserves the same type-5, scaling, board and
text gates as the direct 52-instruction path.

## Pins

`vf2_player_1453c_live` now runs two ROM-backed cases from
`out/park-1442c.vf2snap`:

- direct `r7 == 27`, type-5 index `0x73`: 52 instructions;
- swapped `r7 != 27`, `r8 == 27`, type-5 index `0x73` on the second fighter:
  56 instructions.

Both cases require exact call/return counters and full live-state equality.
Walker misses, scaling branches, text submission and broader `0x1442c`
dispatch variants remain fail-closed.
