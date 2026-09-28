# v0401: recover fa_rob 0x144b0 cmpobl-equal point (r13 == r3)

## Verdict

`hybrid_execute_player_144b0` now admits the `cmpobl`-equal point.
When `0x1450c cmpobl r13, r3` sees `r13 == r3` unsigned (measured with
fighter1 `+0x808 == 1` / `+0x858(f0) == 0` so `r3 = 0` and fighter1
`+0x1aa == 0`), the body takes the same fall-through path as the
v0397 not-taken sibling: `st r5, +0x194(g8)` at `0x14510`,
`b 0x14628` at `0x14514`, and the shared `0x14628` common exit
clearing both `+0x198`. The `+0x654`/`+0x62a` stores and the
`0x14528..0x14560` fighter-state chain are skipped.

Span **47 instructions** from `0x144b0` to `0x1463c` with **+1 call /
+1 return** (the `0x19ef8` call). Final CC = **EQUAL**
(`compare(r13, r3)` with `r13 == r3`; the `b` and exit mov/stores
preserve it), versus GREATER for the `r13 > r3` sibling.

## Measured path

Park `out/park-1442c-s25-nt.vf2snap` (fighter1 `+0x808 == 1`,
`+0x1aa == 100`, `r3 == 0`) with `+0x1aa(f1)` cleared to 0:

```sh
build/Debug/vf2probe --rom-dir roms/vf2 \
  --snapshot out/park-1442c-s25-nt.vf2snap \
  --set-u16 0x512B2A=0 --until 0x1463c
# -> 47 steps, +1 call / +1 return
```

## Accounting

Sibling tail after the `0x19ef8` call: 22 (shared body through
`0x1450c`) + 2 (`0x14510`/`0x14514`) + 5 (common exit) = 29,
identical to the not-taken sibling; only the CC postcondition
differs (EQUAL vs GREATER).

## Unrecovered / out of scope

- The `0x14518` taken arm's neighbors are now complete for the
  measured state-25 shape (taken / not-taken / equal); remaining
  `0x1442c` heavy arms (`0x1453c`/`0x14570`, other `+0x19f` escapes)
  and the `0x14640` state-27/28/bit-4 gates stay fail-closed.

## Pins

- `vf2_player_1442c_live_differential` now runs eleven ROM-backed
  cases including `player-144b0-s25-eq` (47 / +1 / +1, CC = EQUAL),
  all full live-state equal.
- ctest Debug 78/78 with no corridor regressions.
