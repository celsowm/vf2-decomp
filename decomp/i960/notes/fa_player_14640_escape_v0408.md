# v0408: recover the fa_rob escape arm 0x14640 (r198 != 0)

## Verdict

`hybrid_execute_player_14640_escape` now recovers the fa_rob escape arm of
the fighter-state helper `0x14640`, reached when fighter g7 has
`+0x198 != 0`.  The `0x14644 cmpobne 0, r3` then jumps directly to
`0x146dc`.  The arm:

1. `0x14640 ld +0x198(g7), r3` — r3 = the `+0x198` value;
2. `0x14644 cmpobne 0, r3, 0x146dc` — taken (r3 != 0);
3. `0x146dc st r3, +0x194(g7)` — stores r3 to `+0x194(g7)`;
4. `0x146e0 mov 0, r15`;
5. `0x146e4 st r15, +0x654(g7)` — clears `+0x654(g7)`;
   leaves ip at the `0x146e8` ret, unconsumed.

No walker.  Measured short path spans **5** steps from `0x14640` to
`0x146e8` with **+0 call / +0 return**.  Only r3 (= `+0x198`) and r15 (0)
are left distinct from entry; the final reference `compare_result` is LESS
(the `cmpobne 0, r3` at `0x14644` with `+0x198 > 0`).

## Wiring

`hybrid_execute_player_14640` dispatches to the escape arm when
`+0x198 != 0` (after the shared `+0x198` load and before the `+0x654` /
`+0x197` / bit-4 tests), then consumes the `0x146e8` ret to return through
the `0x14640` frame to `0x14438`.  The standalone test entry
`vf2_hybrid_player_14640_escape_execute_for_test` models the arm to the
`0x146e8` boundary without consuming that ret, matching the isolated
`--until 0x146e8` witness exactly.

## Fail-closed

The measured short path requires, on the live park:
- `+0x198(g7) != 0` (`0x14644 cmpobne` taken to `0x146dc`).

Any other shape (`+0x198 == 0`, which flows into the `+0x654` compare
prefix or the shared state/neutral tails) stays `VF2_ERROR_UNSUPPORTED`.

## Reachability note

The `+0x654 != 0` `+0x1aa`/`+0x62a` compare arm at `0x14650`/`0x14658`
(which can also jump to this same `0x146dc` escape when
`s16(+0x1aa) <= s16(+0x62a)`) remains an explicit boundary, as do the
`+0x197 == 27/28` walks, the bit-4-set/bit-4-clear neutral tails and the
`r3 == 13` sibling.

## Pins

- `vf2_player_14640_escape_live_differential` restores
  `out/park-1442c.vf2snap`, forces `+0x198(f0) = 0x7B` at `0x14640`, and
  proves the native arm byte-exact to the `0x146e8` boundary: 5 steps /
  +0 call / +0 return, full live-state equal
  (registers/CC/AC/frames/Work-RAM).
- ROM-independent unit pins: NULL arguments, wrong ip, zero fighter base,
  and no pushed frame all fail closed.
- ctest Debug 88/88 and build-san 88/88 with no corridor regressions.
