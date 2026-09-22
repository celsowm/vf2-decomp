# v0390 — player 0x1428c head 0x1428c→0x142c0 (9247/+6/+6)

## Boundary

`hybrid_execute_player_1428c` previously covered only the setbit-26
store + `+0x10/+0x0c` stores and five `0x27b5c` calls, then published
1682-era poststate (`ip = 0x1428c`, +1622/+4/+4): the `call 0x270d4`
wrapper, the five-slot expansion, the seven-instruction
`0x1429c → 0x142c0` tail (`lda/st/lda/st/ld/ldob/ldob`), the +9235/+5/+5
accounting and all CC/frame/register poststate were missing or stale.

## Witness (measured, fresh build)

- `vf2probe --snapshot out/pre14288.vf2snap --until 0x142c0` →
  **10869 steps / +10 calls / +10 rets** (1622 corridor + 3-insn
  `ld/setbit/st` head + `call` + 9235-insn `0x270d4` wrapper + `ret` +
  7-insn tail). Identical 10869/+10/+10 on boot and natres parks.
- Split: `0x1428c → 0x1429c` = 9240/+6/+6; `0x1429c → 0x142c0` = 7/0/0.
- Head memory: single `ld (g7)` read `0x00000800`, single `st` write
  `0x04000800` (bit 26); five record-selector u16 reads at record
  `0x0201c2fc` (`0x0505/0x0039/0x00f1/0x00e7/0x00af`, v0358/v0359);
  tail stores `+0x10 = 0x00501500`, `+0x0c = 0x000142f4`,
  `g0 = +0x640`, `g1 = +0x04`, `g2 = +0x1b0`.
- No CC write on `ld/setbit/st/lda/ldob/call/ret` under legacy run
  semantics (verified in `executor.c`/`executor_arch.c`: COBR
  `cmpo/cmpi` write CC, `bbs/bbc` overwrite it after legacy runs, plain
  ALU/load/store do not). The span's last CC writer is the `0x270d4`
  wrapper's `cmpdeco` loop → **EQUAL** (probe shows `cmp 2`, AC low 2
  at 0x1429c and 0x142c0; earlier `cmp 3` reading came from a stale
  probe binary).
- Register poststate: `r2 = 0x1429c`, `r15 = 0x142f4`, `g0 = +0x640`,
  `g1 = +0x04`, `g2 = +0x1b0`, `g3 = dest+0x30` (last `0x27b5c` cursor),
  `g5 = 0x50ea98`, `g6 = 0x50e2d0`. Frames unchanged by the span
  (depth 2; `local_frames[2/3]` carry the corridor's own 1428c/2712c
  returns).

## Fix (minimal, fail-closed)

`hybrid_execute_player_1428c` now:

- gates entry on `ip == 0x1428c`, live player, record exactly
  `0x0201c2fc`, scratch nonzero, entry F0 exactly `0x00000800`
  (the corridor's own bit-11 poststate; bit 26 is what the head sets);
- gates each slot on its measured selector
  (`0x0505/0x0039/0x00f1/0x00e7/0x00af`);
- runs the five measured `0x27b5c` expansions (shared helper, already
  pinned by `vf2_player_270d4_five_slot_pin`);
- performs the measured stores and register poststate above;
- pins `EQUAL` via `hybrid_set_compare_result` (AC low lockstep);
- publishes `ip = 0x142c0`, **+9247/+6/+6**;
- zero record/scratch and any unmeasured selector stay
  `VF2_ERROR_UNSUPPORTED`.

New test-only `vf2_hybrid_player_1428c_execute_for_test` wrapper
(declared in `include/vf2/hybrid.h`); the shared dispatch chain still
reaches the head internally from 0x14288 (no new dispatch case).

## Validation

- Extended `test_player_4505_live.c`: same parked machines run the
  corridor (1622/4/4 + equality) then the head unit to 0x142c0
  (10869 total / +10/+10 + full live-state equality).
  `vf2_player_4505_live_tests roms/vf2`: PASS.
- Adjacent: `vf2_player_270d4_five_slot_pin`, `vf2_native_runtime`,
  `vf2_player_planar_rotation`, `vf2_player_28178_stream`: PASS.
- Strict suite: `ctest -C Debug` **74/74 PASS**.

## Status

Native in tree. Next boundary: the `0x142c0` geometry-expansion body
(`call 0x4b838`); `hybrid_execute_player_142c0` (already chained after
the head) is the next verification target.
