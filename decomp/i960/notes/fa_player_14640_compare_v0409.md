# v0409: recover the fa_rob compare-prefix arm 0x14640 (r654 != 0)

## Verdict

`hybrid_execute_player_14640_compare` now recovers the fa_rob
compare-prefix arm of the fighter-state helper `0x14640`, reached when
fighter g7 has `+0x198 == 0` and `+0x654 != 0`.  The `0x1464c cmpobe 0,
r3` is then not taken and the arm runs the `+0x1aa`/`+0x62a` compare
prefix `0x14650`..`0x14658`:

1. `0x14648 ld +0x654(g7), r3` — r3 = the `+0x654` value;
2. `0x1464c cmpobe 0, r3, 0x1465c` — not taken (`+0x654 != 0`);
3. `0x14650 ldos +0x1aa(g7), r13` — r13 = `s16(+0x1aa)`;
4. `0x14654 ldos +0x62a(g7), r14` — r14 = `s16(+0x62a)`;
5. `0x14658 cmpobe r13, r14, 0x146dc` — not taken when
   `s16(+0x1aa) > s16(+0x62a)`, falling through to the shared path at
   `0x1465c`.

On the measured shape (`+0x197` not 27/28/13 and bit 4 of `(g7)` SET) the
neutral bit-4-set tail runs:

6. `0x1465c ldob +0x197(g7), r3` — r3 = `+0x197`;
7. `0x14660/0x1469c cmpobne` — not 27/28, taken;
8. `0x146b0 ld (g7), r15`;
9. `0x146b4 bbc 4, r15` — not taken (bit 4 set);
10. `0x146b8 cmpobe 13, r3` — not taken (`+0x197 != 13`);
11. `0x146bc mov 0, r15 ; 0x146c0 st r15, +0x194(g7)` — clears
    `+0x194(g7)`; leaves ip at the `0x146c4` ret, unconsumed.

No walker.  Measured short path spans **15** steps from `0x14640` to
`0x146c4` with **+0 call / +0 return**.  r3 (= `+0x197`), r13
(= `s16(+0x1aa)`), r14 (= `s16(+0x62a)`) and r15 (0) are left distinct
from entry; the final reference `compare_result` is GREATER (the
`cmpobe 13, r3` at `0x146b8` with r3 < 13).

## Wiring

`hybrid_execute_player_14640` dispatches to the compare-prefix arm when
`+0x198 == 0` and `+0x654 != 0` (after the `+0x198`/`+0x654` loads and
before the `+0x197` / bit-4 tests), then consumes the `0x146c4` ret to
return through the `0x14640` frame to `0x14438`.  The standalone test
entry `vf2_hybrid_player_14640_compare_execute_for_test` models the arm
to the `0x146c4` boundary without consuming that ret, matching the
isolated `--until 0x146c4` witness exactly.

## Fail-closed

The measured short path requires, on the live park:
- `+0x198(g7) == 0` (`0x14644 cmpobne` not taken);
- `+0x654(g7) != 0` (`0x1464c cmpobe` not taken, running the prefix);
- `s16(+0x1aa) > s16(+0x62a)` (`0x14658 cmpobe` not taken);
- `+0x197(g7)` not 27/28/13;
- bit 4 of `(g7)` SET (`0x146b4 bbc` not taken).

The `s16(+0x1aa) <= s16(+0x62a)` jump to the `0x146dc` escape (which
stores the `+0x654` value, a different r3 than the v0408 `+0x198` escape)
stays `VF2_ERROR_UNSUPPORTED`, as do the `+0x197 == 27/28/13` and
bit-4-clear siblings of the compare prefix.

## Reachability note

This completes the `+0x654 != 0` compare-prefix fall-through into the
neutral bit-4-set tail.  The `s16(+0x1aa) <= s16(+0x62a)` escape jump, the
compare-prefix state-27/state-28/bit-4-clear tails and the `r3 == 13`
sibling remain explicit boundaries.

## Pins

- `vf2_player_14640_compare_live_differential` restores
  `out/park-1442c.vf2snap`, forces `+0x654(f0) = 1` and bit 4 of `(g7)`
  set at `0x14640` (with park `s16(+0x1aa) = 1 > s16(+0x62a) = 0`), and
  proves the native arm byte-exact to the `0x146c4` boundary: 15 steps /
  +0 call / +0 return, full live-state equal
  (registers/CC/AC/frames/Work-RAM).
- ROM-independent unit pins: NULL arguments, wrong ip, zero fighter base,
  and no pushed frame all fail closed.
- ctest Debug 90/90 and build-san 90/90 with no corridor regressions.
