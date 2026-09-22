# v0395: recover fa_rob 0x144b0 state-25 collision arm

## Verdict

`hybrid_execute_player_144b0` now natively recovers the measured **state-25
arm** of the fa_rob fighter-exchange body, entered at `0x144b0` when
fighter0's `+0x197 == 25`. On the measured live shape (fighter0 `+0x197 ==
25`, fighter1 `+0x197 == 0`, fighter0 `+0x194 == 0`) the arm:

1. runs the `0x19ef8` **g0 == 0 zero-path** (via the `call 0x19ef8` at
   `0x144b4`), which clears `+0x5cc`/`+0x60c` and `(g7)` bit 9, then
   `+0x1a4 &= 0x00814068` and `+0x1a8 = 0`;
2. runs the collision/state-exchange body: `+0x1aa = 1`, `+0x61e =
   +0x1a8(f1)`, `+0x626 = r4`, `+0x822(f1) = +0x822(f0)`, then computes
   `r5 = 0x11000000 + +0x828(f0)` (altered bit 15), `r3 = r4 - 1`, and
   stores `+0x654(f1) = r5` and `+0x62a(f1) = (u16)r3`;
3. the branch chain (fighter1 state not 27/16) falls through to the
   `0x14628` common exit clearing both fighters' `+0x198`.

Span **53 instructions** from `0x144b0` to `0x1463c` with **+1 call / +1
return** (the `0x19ef8` call; the `0x1463c` ret is not consumed). Final
CC = **GREATER** (last compare `0x14560 cmpobne 16, r8` with `r8 =
+0x197(f1) == 0`).

## Measured paths

Park: `out/park-1442c-s25.vf2snap` (driven from `out/park-1442c.vf2snap`
with fighter0 `+0x197 = 25`, parked at `0x144b0`).

### 0x19ef8 g0 == 0 zero-path (`0x19ef8` / `0x1a11c`)

Entered at `0x144b4` when `g0 = +0x194(g7) == 0`:

- `+0x5cc = 0`, `+0x60c = 0` (prologue).
- `(g7)` bit 9 cleared.
- `cmpobe 0, g0` taken → `0x1a11c`: `+0x1a4 &= 0x00814068`,
  `+0x1a8 = g0 (= 0)`, `ret` to `0x144b8`.
- Body 15 instructions + ret (16 within the call).

### 0x144b0 collision body

- `+0x1aa(f0) = 1`
- `+0x61e(f0) = +0x1a8(f1)`
- `r4 = sext(+0x858(f0)) - sext(+0x808(f1))`; `+0x626(f0) = (u16)r4`
- `+0x822(f1) = +0x822(f0)`
- `r5 = 0x11000000 + sext(+0x828(f0))`, altered bit 15
  (`AC`-dependent; measured clear)
- `r3 = (u32)(r4 - 1)`
- `cmpobl +0x1aa(f1), r3` taken → `0x14518`: `+0x654(f1) = r5`,
  `+0x62a(f1) = (u16)r3`
- branch chain `0x14528/0x1452c/0x14548/0x14560` falls through (fighter1
  state not 27/16) to `0x14628`
- `0x14628/0x1462c` restore g7/g8; `0x14630` `r3 = 0`;
  `0x14634/0x14638` clear `+0x198(f0)`/`+0x198(f1)`.

Final poststate: `r3=0`, `r4=0`, `r5=0x11000000`, `r7=25`, `r8=0`,
`r13=0`, `r14=0`, `r15=0`, `g0=0`, `g7=f0`, `g8=f1`, `CC=GREATER`.

## Accounting

| Path | Instructions (to 0x1463c) | +1 call | +1 return |
|------|---------------------------|---------|-----------|
| state-25 arm | 53 | yes | yes |

`0x144b0`(1) + `0x19ef8` call (call 1 + body 15 + ret 1 = 17) +
`0x144b8..0x14638` (35) = 53.

## Unrecovered / out of scope

- The `0x1442c` full-function integration (reaching `0x144b0` through the
  two `0x14640` calls) requires the `0x14640` state-25 helper path, which
  remains fail-closed. This slice recovers the arm at the arm boundary,
  matching the v0393/v0394 precedent of parking at the helper/arm entry.
- The `0x14510` cmpobl-not-taken sibling, nonzero `+0x194` (`0x19ef8`
  non-zero paths), other `+0x197` values, and the `0x1453c`/`0x14570`
  arms stay fail-closed.
- The `0x14640` state-27/28, bit-4-set, and nonzero `+0x198`/`+0x654`
  gates remain fail-closed.

## Pins

- `vf2_player_1442c_live` now runs three ROM-backed cases: the `0x1442c`
  fast path (51 / +2 / +2), the `0x14640` sibling (14 / +0 / +1), and the
  `0x144b0` state-25 arm (53 / +1 / +1), all full live-state equal.
- ctest Debug **78/78** with no corridor regressions
  (`native-sixth-dispatch`, `native-twelfth-dispatch` unchanged).

## Sanitizer gate

`build-clang-asan` fails to compile the whole project in this environment
(Clang cannot locate standard headers `math.h`/`string.h`, affecting every
translation unit including untouched ones) — a pre-existing toolchain
issue. `build_asan` (MSYS2 GCC) fails on two pre-existing `-Werror`
warnings in unmodified code (`coli_238a4_body` sign-conversion at
`hybrid.c:25359`, unused `hybrid_execute_player_1428c_corridor` at
`hybrid.c:1934`), both present in the committed baseline. The strict MSVC
`build/` gate (`VF2_WARNINGS_AS_ERRORS=ON`) compiles the change cleanly and
passes all 78 tests.

## Next

- Recover the `0x14640` state-25 helper path so the full `0x1442c` body
  can dispatch to `0x144b0` natively.
- Recover the remaining `0x1442c` heavy arms (`0x14518`/`0x1453c`/
  `0x14570`) and the `0x14640` state-27/28/bit-4 gates.
