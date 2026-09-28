# v0405: recover the fa_rob state-27 arm 0x14640 (type-15 walk)

## Verdict

`hybrid_execute_player_14640_state27` now recovers the fa_rob state-27
arm of the fighter-state helper `0x14640`, reached when fighter g7 has
`+0x198 == 0`, `+0x654 == 0` and `+0x197 == 27` (so `0x14660 cmpobne 27,
r3` falls through to the `0x14664` walk).  The arm:

1. `0x14664 ldos +0x194(g7), g0 ; 0x14668 mov 15,g1 ; 0x1466c call 0x1ab34`
   — walks a **type-15** record chain via `+0x194(g7)`;
2. `0x14670 ldos +1(g0), r4 ; 0x14674 mov r4,r4 ; 0x14678 ld 0x500068,r15 ;
   0x14680 bbc 20 (taken) ; 0x14688 subo 1, r4, r4` — computes
   `r4 = s16(+1(record)) - 1` (bit 20 of `0x500068` clear, no `shli`);
3. `0x1468c stos r4, +0x62a(g7)` — stores `r4` (u16) to `+0x62a(g7)`;
4. `0x14690 ld +0x194(g7), r15 ; 0x14694 st r15, +0x654(g7)` — moves the
   original full `+0x194(g7)` u32 into `+0x654(g7)`;
5. `0x146bc mov 0,r15 ; 0x146c0 st r15, +0x194(g7)` — clears `+0x194(g7)`;
6. leaves ip at the `0x146c4` ret, unconsumed.

Measured short path spans **41** steps from `0x14640` to `0x146c4` with
**+1 call / +1 return** (the 0x1ab34 type-15 walker).  The walker restores
the pre-call r0-r15 frame, so only r4/r15/g0/g1 are left distinct from
entry; the final reference `compare_result` is NONE.

## Wiring

`hybrid_execute_player_14640` dispatches to the state-27 arm when
`+0x197 == 27` (after the shared `+0x198 == 0` / `+0x654 == 0` gates) and
then consumes the `0x146c4` ret to return through the `0x14640` frame to
`0x14438`.  The standalone test entry
`vf2_hybrid_player_14640_state27_execute_for_test` models the arm to the
`0x146c4` boundary without consuming that ret, matching the isolated
`--until 0x146c4` witness exactly.

## Fail-closed

The measured short path requires, on the live park:
- `+0x198(g7) == 0` (`0x14644 cmpobne` taken);
- `+0x654(g7) == 0` (`0x1464c cmpobe` taken, skipping the `+0x1aa`/`+0x62a`
  compare arm);
- `+0x197(g7) == 27` (`0x14660 cmpobne 27` fall-through);
- a type-15 walk hit (`g0 != 0` after `0x1ab34`);
- bit 20 of `0x500068` clear (`0x14680 bbc` taken, no `shli`).

Any of these failing, the `+0x654 != 0` `+0x1aa`/`+0x62a` compare arm, or
a type-15 walk miss stays `VF2_ERROR_UNSUPPORTED`.  The `shli 1, r4, r4`
scaling sibling is unmeasured and stays fail-closed.

## Reachability note

This is the natural f0==27 flow: it runs the `0x14640` state-27 path first
(which clears `+0x197` via the 4-byte `+0x194` store), so wiring it makes
the full `0x1442c` f0==27 flow natively reachable.  The state-28
(`0x1469c`) sibling remains an explicit boundary.

## Pins

- `vf2_player_14640_state27_live_differential` restores
  `out/park-1442c.vf2snap`, forces the state-27 shape at `0x14640` with
  `+0x197(f0) = 27` and `+0x194(f0) = 0x73`, and proves the native arm
  byte-exact to the `0x146c4` boundary: 41 steps / +1 call / +1 return,
  full live-state equal (registers/CC/AC/frames/Work-RAM).
- ROM-independent unit pins: NULL arguments, wrong ip, zero fighter base,
  and no pushed frame all fail closed.
- ctest Debug 82/82 and build-san 82/82 with no corridor regressions.
