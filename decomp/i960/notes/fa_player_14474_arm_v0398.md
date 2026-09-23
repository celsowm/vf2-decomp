# v0398: recover fa_rob 0x14474 arm (fighter +0x19f == 25/22)

## Verdict

`hybrid_execute_player_1442c` now natively recovers the measured
**0x14474 arm** of the fa_rob fighter-exchange body. When fighter0
`+0x197 == 24` (taken `cmpobe 24, r7` at `0x14464`) and fighter1
`+0x19f` is 25 (taken `cmpobe` at `0x14478`) or 22 (both compares fall
through), the body stores `0x01000000` to `+0x194(f1)` at `0x14484`,
clears bit 0 of `+0x1a4(f1)` (`ld`/`clrbit`/`st` at
`0x14488..0x14490`), takes `b 0x14628` and clears both fighters'
`+0x198` via the shared common exit.

Spans from `0x1442c` to `0x1463c` (ret not consumed): **54** steps /
+2 calls / +2 rets for `+0x19f == 25`, **55** for `+0x19f == 22` (one
extra not-taken branch). Final CC = **EQUAL** in both cases (taken
`cmpobe 25, r6` / not-taken `cmpobne 22, r6`). Final `r6` is the
`+0x19f` byte, `r7 = 24`, `r8 = +0x197(f1)`, `r14 = +0x19b(f1)`,
`r15` is the cleared `+0x1a4(f1)`, `r3 = 0`, `g7/g8` restored.

## Measured paths

Base park `out/park-1442c.vf2snap` (at `0x1442c`, fighters
`0x510980`/`0x512980` from live registers) with in-test work-RAM
mutations (`+0x197(f0) = 24`, `+0x19f(f1) = 25/22`):

```sh
build/Debug/vf2probe --rom-dir roms/vf2 --snapshot out/park-1442c.vf2snap \
  --set-u8 0x510B17=24 --set-u8 0x512B1F=25 --until 0x1463c
# -> 54 steps, +2 calls / +2 rets
build/Debug/vf2probe --rom-dir roms/vf2 --snapshot out/park-1442c.vf2snap \
  --set-u8 0x510B17=24 --set-u8 0x512B1F=22 --until 0x1463c
# -> 55 steps, +2 calls / +2 rets
```

Reference `--trace` confirms the shape: the first `0x14640` call takes
the `+0x194 != 0` sibling (15 steps incl. call/ret) because `+0x197`
is the high byte of `+0x194` (`24 = 0x18` forces `+0x194(f0) =
0x18000000`), while the second call stays no-op (13 steps). The
`--memory-trace` confirms `+0x194(f1) = 0x01000000` (`00000001` LE),
`+0x1a4(f1)` read/cleared, and both `+0x198` cleared.

## Accounting

Prefix (movs + two helpers + restores): 2 + 15 + 2 + 13 + 2 = 34.
Middle `0x1444c..0x14494`: 15 (`+0x19f == 25`) / 16 (`== 22`).
Common exit `0x14628..0x14638`: 5. Totals 54 / 55.

## Unrecovered / out of scope

- Other `+0x19f` values at `0x14474` (escape to `0x14498`) stay
  fail-closed.
- The `f1 +0x197 == 24` entry (via `0x1446c` movs, measured 60 steps)
  stays fail-closed.
- The `0x1453c`/`0x14570` fighter-state arms, the `0x144b0`
  cmpobl-equal point, and the `0x14640` state-27/28/bit-4 gates stay
  fail-closed.

## Pins

- `vf2_player_1442c_live_differential` now runs six ROM-backed cases:
  fast path (51), `0x14640` sibling (14), state-25 (53), not-taken
  sibling (47), plus the two new `0x14474` arms (54/55), all full
  live-state equal.
- ctest Debug **78/78** with no corridor regressions.
