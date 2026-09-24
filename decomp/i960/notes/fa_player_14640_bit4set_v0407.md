# v0407: recover the fa_rob bit-4-set neutral arm 0x14640 (no walker)

## Verdict

`hybrid_execute_player_14640_bit4set` now recovers the fa_rob bit-4-set
neutral arm of the fighter-state helper `0x14640`, reached when fighter
g7 has `+0x198 == 0`, `+0x654 == 0`, `+0x197` not 27/28 and bit 4 of
`(g7)` SET (so `0x146b0 bbc 4, r15` is not taken), with `r3` (= `+0x197`)
!= 13 (so `0x146b8 cmpobe 13, r3` is also not taken).  The arm:

1. `0x146bc mov 0, r15 ; 0x146c0 st r15, +0x194(g7)` — clears `+0x194(g7)`;
2. leaves ip at the `0x146c4` ret, unconsumed.

No walker.  Measured short path spans **12** steps from `0x14640` to
`0x146c4` with **+0 call / +0 return**.  Only r3 (= `+0x197`) and r15 (0)
are left distinct from entry; the final reference `compare_result` is
GREATER (the `cmpobe 13, r3` at `0x146b8`, not taken with `r3 < 13`).

## Wiring

`hybrid_execute_player_14640` dispatches to the bit-4-set arm when bit 4
of `(g7)` is set and `+0x197` is not 27/28/13 (after the shared
`+0x198 == 0` / `+0x654 == 0` gates), then consumes the `0x146c4` ret to
return through the `0x14640` frame to `0x14438`.  The standalone test
entry `vf2_hybrid_player_14640_bit4set_execute_for_test` models the arm
to the `0x146c4` boundary without consuming that ret, matching the
isolated `--until 0x146c4` witness exactly.

## Fail-closed

The measured short path requires, on the live park:
- `+0x198(g7) == 0` (`0x14644 cmpobne` taken);
- `+0x654(g7) == 0` (`0x1464c cmpobe` taken, skipping the `+0x1aa`/`+0x62a`
  compare arm);
- `+0x197(g7)` not 27/28 (`0x14660`/`0x1469c` both taken);
- bit 4 of `(g7)` SET (`0x146b0 bbc 4, r15` not taken);
- `+0x197` != 13 (`0x146b8 cmpobe 13, r3` not taken).

Any of these failing, the `r3 == 13` sibling (which takes the `0x146c8`
tail), the bit-4-clear no-op/sibling paths, the `+0x654 != 0` `+0x1aa`/
`+0x62a` compare arm, the `+0x197 == 27/28` walks, or the `r198 != 0`
escape stays `VF2_ERROR_UNSUPPORTED`.

## Reachability note

The bit-4-clear neutral tail was already native (v0393/v0394 no-op and
sibling paths).  This slice completes the bit-4-set neutral shape for
`+0x197` not 27/28/13.  The `r3 == 13` sibling, the `+0x654 != 0`
`+0x1aa`/`+0x62a` compare arm and the `r198 != 0` escape remain explicit
boundaries.

## Pins

- `vf2_player_14640_bit4set_live_differential` restores
  `out/park-1442c.vf2snap`, forces the bit-4-set shape at `0x14640` with
  `+0x197(f0) = 0` and bit 4 of `(g7)` set, and proves the native arm
  byte-exact to the `0x146c4` boundary: 12 steps / +0 call / +0 return,
  full live-state equal (registers/CC/AC/frames/Work-RAM).
- ROM-independent unit pins: NULL arguments, wrong ip, zero fighter base,
  and no pushed frame all fail closed.
- ctest Debug 86/86 and build-san 86/86 with no corridor regressions.
