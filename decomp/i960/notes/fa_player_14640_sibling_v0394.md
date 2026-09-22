# v0394: recover fa_rob 0x14640 +0x194 != 0 sibling

## Verdict

`hybrid_execute_player_14640` now natively recovers a second measured
sibling of the fa_rob collision/state helper: the `+0x194 != 0` path.
Together with the v0393 no-op path (`+0x194 == 0`), the helper's shared
gate set (`+0x198 == 0`, `+0x654 == 0`, `+0x197` not 27/28, `(g7)` bit 4
clear) now dispatches to both measured exits.

This was measured while scouting the `0x1442c` state-25 collision arm
(`0x144b0`), where fighter0 carries a nonzero `+0x194` and the first
`0x14640` call takes this sibling instead of the no-op.

## Measured paths

Park: `out/park-14640-sib.vf2snap` (from `out/park-1442c.vf2snap` with
`--set-ip 0x14640 --set-u32 0x510b14=0x1`), g7 = fighter0 = 0x510980.

### +0x194 != 0 sibling

`mov 0,r15; st r15,+0x654(g7); ret` (`0x146d0`/`0x146d4`/`0x146d8`).

- Leaves `r15 = 0`, `r14 = +0x194` (nonzero), `r3 = +0x197`.
- `CC = LESS` (from `cmpobe 0, r14` with `+0x194 != 0`).
- Writes `+0x654 = 0`.
- Body 13 instructions to `0x146d8` + ret = 14 total, `+1` return.

### no-op (v0393, unchanged)

`+0x194 == 0`: leaves `r15 = (g7)` flags, `CC = EQUAL`, 11 + ret.

## Accounting

| Path | Instructions (to ret) | +1 return |
|------|----------------------|-----------|
| no-op (`+0x194 == 0`) | 12 | yes |
| sibling (`+0x194 != 0`) | 14 | yes |

## Unrecovered / out of scope

- `0x14640` state-27/28 arms, bit-4-set, and nonzero `+0x198`/`+0x654`
  gates remain fail-closed.
- The full `0x1442c` state-25 arm (`0x144b0`, which calls `0x19ef8`)
  is scouted but not yet recovered; its `+0x194` word/halfword and
  `+0x197` field overlap make the byte-exact differential delicate and
  it is deferred.

## Pins

- `vf2_player_1442c_live_differential` now runs two ROM-backed cases:
  the `0x1442c` fast path (51 / +2 / +2) and the `0x14640` sibling
  (14 / +0 / +1), both full live-state equal.
- ctest Debug **78/78**.

## Next

- Recover the `0x1442c` state-25 arm (`0x144b0`) by pinning the exact
  `+0x194`/`+0x197` field layout, then driving the `0x19ef8` zero-path
  call and the collision-exchange body.
