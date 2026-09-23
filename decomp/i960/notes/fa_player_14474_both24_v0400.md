# v0400: recover fa_rob 0x14474 both-24 entry (f0 +0x197 == 24, f1 +0x197 == 24)

## Verdict

`hybrid_execute_player_1442c` now admits the both-`== 24` shape on the
measured direct path. When fighter0 `+0x197 == 24`, the `cmpobe 24, r7`
at `0x14464` is taken to `0x14474` regardless of fighter1 `+0x197`
(f0 priority); the same `+0x19f(f1)` body runs on fighter1: it stores
`0x01000000` to `+0x194(f1)`, clears bit 0 of `+0x1a4(f1)` and rejoins
the `0x14628` common exit clearing both `+0x198`.

Spans from `0x1442c` to `0x1463c`: **56** steps / +2 calls / +2 rets
for `+0x19f(f1) == 25`, **57** for `== 22`. Final CC = **EQUAL**.
The prefix is 36 steps (both `0x14640` helpers take the `+0x194 != 0`
sibling at 15 steps each since `+0x197` is the high byte of `+0x194`);
the middle `0x1444c..0x14494` span stays 15/16 and the exit 5, so no
new accounting constant was needed beyond relaxing the direct gate.

## Measured paths

Base park `out/park-1442c.vf2snap` with in-test work-RAM mutations
(`+0x197(f0) = 24`, `+0x197(f1) = 24`, `+0x19f(f1) = 25/22`):

```sh
build/Debug/vf2probe --rom-dir roms/vf2 --snapshot out/park-1442c.vf2snap \
  --set-u8 0x510B17=24 --set-u8 0x512B17=24 --set-u8 0x512B1F=25 \
  --until 0x1463c
# -> 56 steps, +2 calls / +2 rets
build/Debug/vf2probe --rom-dir roms/vf2 --snapshot out/park-1442c.vf2snap \
  --set-u8 0x510B17=24 --set-u8 0x512B17=24 --set-u8 0x512B1F=22 \
  --until 0x1463c
# -> 57 steps, +2 calls / +2 rets
```

## Accounting

Prefix 36 (2 + 15 + 2 + 15 + 2). Middle 15 (`== 25`) / 16 (`== 22`).
Common exit 5. Totals 56 / 57.

## Unrecovered / out of scope

- Other `+0x19f` values on any 0x14474 entry (escape to `0x14498`) stay
  fail-closed.
- The `0x1453c`/`0x14570` fighter-state arms, the `0x144b0`
  cmpobl-equal point, and the `0x14640` state-27/28/bit-4 gates stay
  fail-closed.

## Pins

- `vf2_player_1442c_live_differential` now runs ten ROM-backed cases:
  fast path (51), `0x14640` sibling (14), state-25 (53), not-taken
  sibling (47), direct `0x14474` arms (54/55), swapped arms (57/58)
  and both-24 arms (56/57), all full live-state equal.
- ctest Debug 78/78 with no corridor regressions (twelfth-dispatch
  re-passes on retry).
