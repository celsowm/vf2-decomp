# v0406: recover the fa_rob state-28 arm 0x14640 (no walker)

## Verdict

`hybrid_execute_player_14640_state28` now recovers the fa_rob state-28
arm of the fighter-state helper `0x14640`, reached when fighter g7 has
`+0x198 == 0`, `+0x654 == 0` and `+0x197 == 28` (so `0x14660 cmpobne 27,
r3` jumps to `0x1469c` and `0x1469c cmpobne 28, r3` falls through).  The
arm:

1. `0x146a0 ldos +0x1aa(g7), r3 ; 0x146a4 addo 3, r3, r3 ;
   0x146a8 stos r3, +0x1aa(g7)` — adds 3 to `s16(+0x1aa(g7))` and stores
   the u16 back to `+0x1aa(g7)`;
2. `0x146bc mov 0, r15 ; 0x146c0 st r15, +0x194(g7)` — clears `+0x194(g7)`;
3. leaves ip at the `0x146c4` ret, unconsumed.

No walker.  Measured short path spans **13** steps from `0x14640` to
`0x146c4` with **+0 call / +0 return**.  Only r3 (the new `+0x1aa` value)
and r15 (0) are left distinct from entry; the final reference
`compare_result` is EQUAL (the `cmpobne 28, r3` at `0x1469c`, not taken).

## Wiring

`hybrid_execute_player_14640` dispatches to the state-28 arm when
`+0x197 == 28` (after the shared `+0x198 == 0` / `+0x654 == 0` gates) and
then consumes the `0x146c4` ret to return through the `0x14640` frame to
`0x14438`.  The standalone test entry
`vf2_hybrid_player_14640_state28_execute_for_test` models the arm to the
`0x146c4` boundary without consuming that ret, matching the isolated
`--until 0x146c4` witness exactly.

## Fail-closed

The measured short path requires, on the live park:
- `+0x198(g7) == 0` (`0x14644 cmpobne` taken);
- `+0x654(g7) == 0` (`0x1464c cmpobe` taken, skipping the `+0x1aa`/`+0x62a`
  compare arm);
- `+0x197(g7) == 28` (`0x1469c cmpobne 28` fall-through).

Any of these failing, the `+0x654 != 0` `+0x1aa`/`+0x62a` compare arm, the
`+0x197 == 27` walk, or the `+0x197` not 27/28 neutral tail
(`0x146b0`/`0x146c8`/`0x146dc`) stays `VF2_ERROR_UNSUPPORTED`.

## Reachability note

Together with the v0405 state-27 walk, both state-27 (`+0x197 == 27`) and
state-28 (`+0x197 == 28`) sub-arms of the `0x14640` helper are now native,
so the full `0x1442c` f0 == 27/28 flow is natively reachable.  The
`+0x654 != 0` `+0x1aa`/`+0x62a` compare arm and the `+0x197` not 27/28
neutral tail (`0x146b0`/`0x146c8`/`0x146dc`) remain explicit boundaries.

## Pins

- `vf2_player_14640_state28_live_differential` restores
  `out/park-1442c.vf2snap`, forces the state-28 shape at `0x14640` with
  `+0x197(f0) = 28`, and proves the native arm byte-exact to the `0x146c4`
  boundary: 13 steps / +0 call / +0 return, full live-state equal
  (registers/CC/AC/frames/Work-RAM).
- ROM-independent unit pins: NULL arguments, wrong ip, zero fighter base,
  and no pushed frame all fail closed.
- ctest Debug 84/84 and build-san 84/84 with no corridor regressions.
