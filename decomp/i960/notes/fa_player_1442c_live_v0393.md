# v0393: recover fa_rob 0x1442c fighter-exchange body (live fast path)

## Verdict

`hybrid_execute_player_1442c` now natively recovers the measured live
fast path of the fa_rob fighter-exchange body at `0x1442c`, together with
its two `0x14640` no-op helper calls (`hybrid_execute_player_14640`).
This is the first native block of the collision/state-exchange function
that immediately follows the recovered player corridor, extending the
playable-match frontier toward fighter collision.

The 0x1442c function is called at `0x14388` when the instance byte
`+0x04(g7) == 0`.  On the accepted live shape both fighters are in a
neutral state (`+0x197` not in {16, 24, 25, 27}), so the body runs the two
0x14640 helper calls with swapped g7/g8, the state-byte checks escape to
the `0x14628` common exit, and both fighters' `+0x198` are cleared to 0.

## Measured paths

Park: `out/park-1442c.vf2snap`, driven from `out/pre14288.vf2snap` with
`vf2probe --until 0x1442c` (10949 steps).

### 0x14640 no-op helper (called twice, once per fighter)

No-op when: `+0x198 == 0`, `+0x654 == 0`, `+0x197` not 27/28, `(g7)` bit 4
clear, `+0x194 == 0`.  It leaves `r3 = +0x197`, `r14 = +0x194`,
`r15 = (g7)` flags and `CC = EQUAL` (from `cmpobe 0, r14`), then rets.

Body: 12 instructions to the ret (11 + ret), +1 return.

### 0x1442c body (full function)

1. `mov g7,r10` / `mov g8,r11` (save originals)
2. `call 0x14640` (g7 = fighter0) -> no-op ret
3. `mov r11,g7` / `mov r10,g8` (swap)
4. `call 0x14640` (g7 = fighter1) -> no-op ret
5. `mov r10,g7` / `mov r11,g8` (restore)
6. State-byte checks on `+0x19b` / `+0x197` (compare 16/24/25/27) -> all
   escape to `0x14628` (both fighters neutral)
7. `0x14628`: `mov r10,g7` / `mov r11,g8` / `mov 0,r3`
   / `st r3,+0x198(g7)` / `st r3,+0x198(g8)`
8. `0x1463c` ret (not consumed by this native body; the caller continues
   at its `0x1438c` return address)

Full-function reference totals to `ip == 0x1463c` (the ret instruction):

| Span | Instructions | Calls | Returns |
|------|--------------|-------|---------|
| 0x1442c -> 0x1463c | 51 | +2 | +2 |

Final `CC = GREATER` (from the last compare `0x14560 cmpobne 16, r8` with
`r8 = +0x197 == 0`, i.e. `compare(16, 0)`).  The body clears both
fighters' `+0x198` and restores `g7/g8` to the original fighter bases.

## Accounting

- `0x1442c` non-call body: 25 instructions.
- Two `0x14640` calls: each `+1` (call) + `11` (body) + `1` (ret) = 13.
- Total to `0x1463c`: 25 + 13 + 13 = 51; `+2` calls, `+2` returns.
- The final `0x1463c` ret is not consumed, so the caller continues at
  `0x1438c`.

## Unrecovered / out of scope

- Heavy collision branches: `0x144b0` (call `0x19ef8` + collision write),
  `0x14518` (`st r5,+0x654(g8)`), and the `0x1453c`/`0x14570` arms
  (fighter states 27/16 / board bit 5).  All fail closed.
- `0x14640` non-no-op branches (states 27/28, bit 4 set, nonzero
  `+0x198`/`+0x654`/`+0x194`) fail closed.
- The `0x1442c` path reached when `+0x04(g7) != 0` (the call at `0x14388`
  is skipped on the accepted corridor) is not part of this slice.

## Tooling

Measurement reused `vf2probe` (reference interpreter) from the accepted
`pre14288` live corridor.  No new tooling.

## Pins

- `vf2_player_1442c_live_differential`: ROM-backed, full live-state
  equality at `0x1463c` (51 steps, +2 calls / +2 rets).
- `vf2_player_1442c_live`: ROM-independent invalid-argument unit test.
- ctest Debug **78/78** (was 76/76), including `native-sixth-dispatch` and
  `native-twelfth-dispatch` unchanged.

## Next

- Recover the `0x1442c` heavy collision arms (`0x144b0`/`0x14518`/
  `0x14570`) by driving a shape where a fighter `+0x197` is 16/24/25/27
  or `+0x04(g7) != 0`.
- The `0x19ef8` flag-bit siblings and post-`0x28780` geometry helpers
  remain the other open frontiers.
