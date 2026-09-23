# v0399: recover fa_rob 0x14474 swapped entry (f1 +0x197 == 24)

## Verdict

`hybrid_execute_player_1442c` now natively recovers the swapped entry
to the measured 0x14474 arm. When fighter0 `+0x197 != 24` but fighter1
`+0x197 == 24`, the `cmpobe 24, r7` falls through and `cmpobne 24, r8`
falls through to the `0x1446c`/`0x14470` swap (`g7 = f1`, `g8 = f0`),
and the same `+0x19f` body runs with `g8 = fighter0`: it stores
`0x01000000` to `+0x194(f0)`, clears bit 0 of `+0x1a4(f0)` and rejoins
the `0x14628` common exit clearing both `+0x198`.

Spans from `0x1442c` to `0x1463c` (ret not consumed): **57** steps /
+2 calls / +2 rets for `+0x19f(f0) == 25`, **58** for `== 22`.
Final CC = **EQUAL**. Final `r6` is the `+0x19f(f0)` byte, `r7 =
+0x197(f0)`, `r8 = 24`, `r14 = +0x19b(f1)`, `r15` is the cleared
`+0x1a4(f0)`, `r3 = 0`, `g7/g8` restored to the original bases.

## Measured paths

Base park `out/park-1442c.vf2snap` with in-test work-RAM mutations
(`+0x197(f1) = 24`, `+0x19f(f0) = 25/22`):

```sh
build/Debug/vf2probe --rom-dir roms/vf2 --snapshot out/park-1442c.vf2snap \
  --set-u8 0x510B17=0 --set-u8 0x512B17=24 --set-u8 0x510B1F=25 \
  --until 0x1463c
# -> 57 steps, +2 calls / +2 rets
build/Debug/vf2probe --rom-dir roms/vf2 --snapshot out/park-1442c.vf2snap \
  --set-u8 0x510B17=0 --set-u8 0x512B17=24 --set-u8 0x510B1F=22 \
  --until 0x1463c
# -> 58 steps, +2 calls / +2 rets
```

Reference `--trace` confirms the swap (`0x1446c mov r11,g7`,
`0x14470 mov r10,g8`) and the body on fighter0. `--memory-trace`
confirms `+0x194(f0) = 0x01000000`, the `+0x1a4(f0)` read/modify/write
(`0x00020000` preserved, bit 0 already clear) and both `+0x198`
cleared. The first `0x14640` call is no-op (13 steps) and the second
takes the `+0x194 != 0` sibling (15 steps), mirroring the v0398
distribution.

## Accounting

Prefix 34 (2 + 13 + 2 + 15 + 2). Middle `0x1444c..0x14494`: 18
(`+0x19f == 25`: 10 to the swap + 2 compares + 6 body) / 19
(`== 22`). Common exit 5. Totals 57 / 58.

## Unrecovered / out of scope

- Other `+0x19f` values on either entry (escape to `0x14498`) stay
  fail-closed.
- Both fighters `+0x197 == 24` (f0-priority path) stays fail-closed.
- The `0x1453c`/`0x14570` fighter-state arms, the `0x144b0`
  cmpobl-equal point, and the `0x14640` state-27/28/bit-4 gates stay
  fail-closed.

## Pins

- `vf2_player_1442c_live_differential` now runs eight ROM-backed cases:
  fast path (51), `0x14640` sibling (14), state-25 (53), not-taken
  sibling (47), direct `0x14474` arms (54/55) and swapped `0x14474`
  arms (57/58), all full live-state equal.
- ctest Debug **78/78** with no corridor regressions.
