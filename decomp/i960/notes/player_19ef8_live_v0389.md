# v0389 — player live 0x505 corridor 0x14288→0x1428c (1622/4/4)

## Witness (measured, current build)

- `vf2probe --snapshot out/pre14288.vf2snap --until 0x1428c` → 1622
  steps, 4 calls / 4 rets (calls `0x19ef8`, `0x1a1e4`, `0x26ef0`,
  `0x27130`; rets 4; tail `ret 0x1428c` via `0x1a118 → 0x1a134`).
- Entry `+0x1a4 == 0`, CC=NONE, AC low clear; selector `g0 == 0x505`
  (ROM `ldos (g6), g0` at `0x14280`; bit 14 clear so `bbc 14` at
  `0x19f2c` skips the clrbit-6/5/21 prologue).
- Final `+0x1a4 == 0x200`, F0 `0x04000800`-family; finals match on
  punch10/boot/natres parks.

## Root causes fixed (all measured against the trace)

- Status-byte reuse: the `0x26f20` loop reads ONE status byte per
  outer iteration (`ldob (r9)` at `0x26f24`, r9 advancing at `0x26f6c`)
  and emits three expansion bytes from shifts 0/2/4; the per-inner
  dispatch is on the top two bits (`shro 6`/`and 3`, mode-1 on
  nonzero). An earlier draft re-read the byte per inner pass AND
  gated mode-0-only, diverging at outer 17 (`raw 0x80`).
- `g2` publish: ROM `0x26fa0 mov r9,g2` publishes the post-loop r9
  (`table + 22`) plus one per census `ldob (g2)` at `0x27090` (5x
  live → final `0x217d0c3 = table + 27`, gated on `scratch_count`).
- Census float arm: fires on census-outer 17 (`subo 12,r3 → r4 = 5`);
  jump word `0x29724[5] = 0x16e`; dest `player + 0x16e`; `cvtri/stos`
  triple at `0x217d2ac` (`0, 0, 0x46800013 → 0, 0, 0x4000`,
  window `00 00 00 40`); `+0xbdc = 0x20`. The census `stob 0,+0xbdc`
  prologue was wrongly ordered after the census (clobbering `0x20`).
- Opcode dispatch: `bx` table `0x1a350` sends opcode 1 → `0x1a408`
  (two `ldib/stib` to `+0x802/+0x803`, `r11 += 3`, loop) and opcode 8
  → `0x1a398` (`addo 1,r11`, stores to `+0x82c/+0x6d0 = 0x200e8b3`).
  Live stream is opcode 1 (`01 ff ff`) then opcode 8; the stale
  opcode-8 handler copied `(r11)+1/+2` and overwrote `ff/ff` with
  `12/01` (measured diff `0x511182`). Opcode 8 now stores nothing.
- Ghost `0x4505`: a forced-g0 park that never occurs live (faults at
  the `0x27048 cvtri` on every drive); stays fail-closed.

## Scope

- `hybrid_execute_player_19ef8` only, selector `0x505` (+ `0x284`
  unchanged), gated on the measured expansion histogram (5/52/3),
  stream bytes, jump word, float triple, census counts.
- Shared `player_selector_setup.inc` change is one measured fix:
  opcode 8 performs no fighter stores (opcode-1 handler already
  covers the `0x1a408` copies). Planar rotation unit tests still pass.
- Test-only `vf2_hybrid_player_19ef8_execute_for_test` wrapper +
  snapshot fixture `tests/recovered/test_player_4505_live.c`
  (restores `out/pre14288.vf2snap`, no synthetic seed): 1622/4/4 +
  full live-state equality.

## Validation

- `vf2_player_4505_live_tests roms/vf2`: PASS (1622/4/4, diff equal).
- `vf2_player_planar_rotation_tests`: PASS.
- Strict suite: `ctest -C Debug` 74/74 PASS.

## Status

Native in tree (minimal measured-shape admissions, fail-closed
siblings untouched). The frozen `0x1428c` head (setbit-26 + `27b5c`
fanout) remains the next boundary. The removed corridor-only dispatch
arm (double-counted body, bypassed `hybrid_complete_procedure`) is
documented in the test header; the parked entry routes through the
shared chain gates.
