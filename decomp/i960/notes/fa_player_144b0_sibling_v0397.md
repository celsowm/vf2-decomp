# v0397: recover fa_rob 0x144b0 cmpobl-not-taken sibling

## Verdict

`hybrid_execute_player_144b0` now natively recovers the measured
**cmpobl-not-taken sibling** of the fa_rob state-25 collision arm. When
`0x1450c cmpobl r13, r3` does not take (`r13 > r3` unsigned), the body
runs `st r5, +0x194(g8)` at `0x14510`, `b 0x14628` at `0x14514`, and the
shared `0x14628` common exit clearing both fighters' `+0x198`. The
`+0x654`/`+0x62a` stores and the `0x14528..0x14560` fighter-state chain
are skipped.

Span **47 instructions** from `0x144b0` to `0x1463c` with **+1 call / +1
return** (the `0x19ef8` call; the `0x1463c` ret is not consumed). Final
CC = **GREATER** (the not-taken `cmpobl` leaves `compare(r13, r3)` with
`r13 > r3`; the `b` and the exit mov/stores preserve it).

## Measured paths

Park: `out/park-1442c-s25-nt.vf2snap`, created from
`out/park-1442c-s25.vf2snap` (already at `0x144b0`) with zero-step
mutation capture:

```sh
build/Debug/vf2probe --rom-dir roms/vf2 \
  --snapshot out/park-1442c-s25.vf2snap \
  --set-u16 0x513188=1 --set-u16 0x512B2A=100 \
  --until 0x144b0 --output-snapshot out/park-1442c-s25-nt.vf2snap
```

(fighter1 `0x512980`: `+0x808 = 1`, `+0x1aa = 100`; everything else
identical to the v0395 shape: fighter0 `+0x197 == 25`, fighter1
`+0x197 == 0`, fighter0 `+0x194 == 0`).

Reference `--memory-trace` from the new park to `0x1463c`:

- prefix identical to v0395 (19ef8 zero-path, `+0x1aa(f0) = 1`,
  `+0x61e(f0) = +0x1a8(f1)`, `+0x626(f0) = 1`, `+0x822(f1) = +0x822(f0)`);
- `0x1450c cmpobl` falls through to `0x14510` (`83212 -> 83216`);
- word store `+0x194(f1) = 0x11000000` (`5319444`, bytes `00000011`);
- `b 0x14628`, then the common exit clears `+0x198(f0)`/`+0x198(f1)`;
- `+0x654(f1)`/`+0x62a(f1)` never written on this path;
- `47` steps / `+1` call / `+1` return (snap baseline `11026/11023`).

## Oracle-semantics corrections (behavior-identical on the old pin)

Driving a nonzero shape exposed two latent modeling errors in the v0395
code that cancel out on the all-zero park:

- ROM `ldos` **zero-extends** (`executor.c`: `ldos` -> `execute_load(...,
  2u, false)`; `ldis` is the sign-extending form). The arm now
  zero-extends `+0x858`/`+0x808`/`+0x828`/`+0x1aa`.
- `subi r13, r14, r4` computes `r4 = r14 - r13`, i.e.
  `+0x808(f1) - +0x858(f0)` (i960 subtract order `dst = src2 - src1`,
  confirmed by the mutation matrix: setting `+0x808(f1) = 1` yields
  `r4 = 1`, `r3 = 0`). The old code had the operands backwards.

Both corrections are exact on the v0395 pin (all halves zero) and are
required for any nonzero `+0x858`/`+0x808`/`+0x1aa` shape.

## Accounting

| Path | Instructions (to 0x1463c) | +1 call | +1 return |
|------|---------------------------|---------|-----------|
| state-25 arm (taken) | 53 | yes | yes |
| cmpobl-not-taken sibling | 47 | yes | yes |

Sibling tail after the `0x19ef8` call: 22 (shared body through
`0x1450c`) + 2 (`0x14510`/`0x14514`) + 5 (common exit) = 29.

## Unrecovered / out of scope

- The `cmpobl`-equal point (`r13 == r3`) stays fail-closed; only the
  measured `r13 > r3` (GREATER) exit is admitted.
- The `0x14518` arm's `0x14510`-style neighbors are done; remaining
  `0x1442c` heavy arms (`0x1453c`/`0x14570`, the `0x14474` `+0x19f`
  path) and the `0x14640` state-27/28/bit-4 gates stay fail-closed.
- The full-`0x1442c` state-25 dispatch (v0396) inherits the sibling
  through `hybrid_execute_player_144b0`; no separate full-dispatch
  sibling fixture is added.

## Pins

- `vf2_player_1442c_live` now runs four ROM-backed cases: the `0x1442c`
  fast path (51 / +2 / +2), the `0x14640` sibling (14 / +0 / +1), the
  `0x144b0` state-25 arm (53 / +1 / +1), and the new not-taken sibling
  (47 / +1 / +1), all full live-state equal.
- ctest Debug **78/78** with no corridor regressions
  (`native-sixth-dispatch`, `native-twelfth-dispatch` unchanged).

## Next

- Recover the `0x14474` arm (fighter `+0x19f == 25`: `+0x194(g8) =
  0x01000000`, clear `+0x1a4(g8)` bit 0, join `0x14628`) and the
  `0x1453c`/`0x14570` fighter-state arms.
- Recover the `0x14640` state-27/28 and bit-4 gates.
