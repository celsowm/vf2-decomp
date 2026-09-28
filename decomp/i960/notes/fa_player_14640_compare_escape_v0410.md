# v0410: recover the fa_rob compare-prefix escape arm 0x14640 (s16(+0x1aa) == s16(+0x62a))

## Verdict

`hybrid_execute_player_14640_compare_escape` now recovers the fa_rob
compare-prefix escape arm of the fighter-state helper `0x14640`, reached
when fighter g7 has `+0x198 == 0` and `+0x654 != 0` and
`s16(+0x1aa) == s16(+0x62a)`.  The `0x1464c cmpobe 0, r3` is then not
taken, the `+0x1aa`/`+0x62a` compare prefix `0x14650`..`0x14658` runs, and
the `0x14658 cmpobe r13, r14` IS taken (r13 == r14) to `0x146dc`:

1. `0x14648 ld +0x654(g7), r3` — r3 = the `+0x654` value;
2. `0x1464c cmpobe 0, r3, 0x1465c` — not taken (`+0x654 != 0`);
3. `0x14650 ldos +0x1aa(g7), r13` — r13 = `s16(+0x1aa)`;
4. `0x14654 ldos +0x62a(g7), r14` — r14 = `s16(+0x62a)`;
5. `0x14658 cmpobe r13, r14, 0x146dc` — taken (`s16(+0x1aa) == s16(+0x62a)`);
6. `0x146dc st r3, +0x194(g7)` — stores r3 (= the `+0x654` value) to
   `+0x194(g7)`;
7. `0x146e0 mov 0, r15`;
8. `0x146e4 st r15, +0x654(g7)` — clears `+0x654(g7)`;
   leaves ip at the `0x146e8` ret, unconsumed.

No walker.  Measured short path spans **10** steps from `0x14640` to
`0x146e8` with **+0 call / +0 return**.  r3 (= the `+0x654` value), r13
(= `s16(+0x1aa)`), r14 (= `s16(+0x62a)`) and r15 (0) are left distinct from
entry; the final reference `compare_result` is EQUAL (the `cmpobe r13, r14`
at `0x14658` with r13 == r14).

## Wiring

`hybrid_execute_player_14640` dispatches to the compare-prefix escape arm
when `+0x198 == 0`, `+0x654 != 0` and `s16(+0x1aa) == s16(+0x62a)` (after
the `+0x198`/`+0x654` loads and the `+0x1aa`/`+0x62a` read, and before the
`+0x197` / bit-4 tests), then consumes the `0x146e8` ret to return through
the `0x14640` frame to `0x14438`.  The standalone test entry
`vf2_hybrid_player_14640_compare_escape_execute_for_test` models the arm
to the `0x146e8` boundary without consuming that ret, matching the
isolated `--until 0x146e8` witness exactly.

## Fail-closed

The measured short path requires, on the live park:
- `+0x198(g7) == 0` (`0x14644 cmpobne` not taken);
- `+0x654(g7) != 0` (`0x1464c cmpobe` not taken, running the prefix);
- `s16(+0x1aa) == s16(+0x62a)` (`0x14658 cmpobe` taken).

The `s16(+0x1aa) != s16(+0x62a)` fall-through (handled by the v0409
compare arm when `s16(+0x1aa) > s16(+0x62a)`) and the `+0x198 != 0` escape
(v0408) stay `VF2_ERROR_UNSUPPORTED` from this entry.

## Reachability note

The `cmpobe` at `0x14658` branches on **equality** (`r13 == r14`), not
`<=`; the `s16(+0x1aa) < s16(+0x62a)` shape is a fall-through that the
v0409 compare arm currently fails closed on (unmeasured sibling).  The
compare-prefix state-27/state-28/bit-4-clear/bit-4-set neutral tails and
the `r3 == 13` sibling remain explicit boundaries.

## Pins

- `vf2_player_14640_compare_escape_live_differential` restores
  `out/park-1442c.vf2snap`, forces `+0x654(f0) = 5` and
  `+0x62a(f0) = 1` (equal to park `s16(+0x1aa) = 1`) at `0x14640`, and
  proves the native arm byte-exact to the `0x146e8` boundary: 10 steps /
  +0 call / +0 return, full live-state equal
  (registers/CC/AC/frames/Work-RAM).
- ROM-independent unit pins: NULL arguments, wrong ip, zero fighter base,
  and no pushed frame all fail closed.
- ctest Debug 92/92 and build-san 92/92 with no corridor regressions.
