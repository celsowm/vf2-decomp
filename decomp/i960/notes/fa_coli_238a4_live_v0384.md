# v0384 — coli g3-scan 0x238a4 live (f0/f1 whole-task)

**Status:** recovered C + ROM-backed differential green; committed.

Reopens `fa_coli_238a4` warm-only (v0279) and whole-task gate (v0349/v0352).

## 1. Witness

Whole-task parked `out/coli-parked-221e8.vf2snap` at `0x221e8`:

* warm `9214/18/19` (both fighters `+0x1a4` bit8 clear)
* f0 `9393/17/18` (`0x510b24=0x100`, fighter0 bit8)
* f1 `9385/17/18` (`0x512b24=0x100`, fighter1 bit8) — mirrored sibling,
  previously fail-closed at the whole-task gate
* both `9528/18/19` (`both 0x100`)

The whole-task shell `0x23524` contains two `0x238a4` invocations.
Warm is 5 steps each (mov,mov,ld,bbc,ret, `g3=0`).
Live single-fighter takes the `0x1aa/0x808` compare + `0x23284`
30-trip `setbit` loop:

* f0 first `0x238a4` (fighter0 live) 136 steps, `g3=0x18`, `CC=EQUAL`
* f0 second warm 5 steps, `g3=0`, `CC=NONE`
* f1 first warm 5 steps
* f1 second live 134 steps, `g3=0`, `CC=EQUAL` (2 fewer `setbit`s)

Total shell live `+131` vs warm for f0 (`+129` for f1), hence whole
`9393`/`9385` vs `9214`.

## 2. Delta vs warm

* `coli_233d0_body` (flag builder) previously failed closed on any
  `bit8` (`xor`/`and`).  The ROM at `0x23408` handles `xor bit8`
  by checking `0x820==41` for both fighters and setting `g6 bit1`
  only when both are 41; otherwise `g6` stays 0.  For the
  measured whole-task live (`0x820==1`, not 41) `g6` stays 0, so
  the warm table `0x232c4` is still used.  The builder now allows
  `xor bit8` with that `0x820` check, `and bit8` would set `g6`
  bit2 (still fail-closed), `bit14` would set bit3 (fail-closed).

* `coli_238a4_body` now has a live path:
  `ldos 0x1aa/0x808` (signed), `cmpobl` (unsigned `<`), `ldob
  0x820`/`addo 41`/`cmpobe`, `ld 0x2007aca[820]`, `lda 0x23284`,
  30-trip `ldob`/`bbc`/`setbit`/`cmpinco`/`bne`, `ret`.
  Early exits (`<` and `==41`) stay fail-closed for now; the
  measured live shape (`0,0,1`) takes neither.  Body is counted
  per instruction (`4` warm, `7`/`10` early, `135`/`133` live
  +1 ret = `5`/`136`/`134`).  `g3` is the `setbit` mask, `CC`
  is `EQUAL` for the live single-fighter.

* Shell body accounting now adds `b238+1` per `0x238a4` instead of
  fixed `5`, so whole-task `9393`/`9385` are correctly counted.

* Whole-task gate at `hybrid_execute_coli_body` now allows
  `9385/17/18` alongside `9393`/`9528`/`9214`.  The full
  `0x23524` live shell still needs the `0x2396c`/`0x233d0`
  live tables for the `g4`/`0x10c` etc, so `9385`/`9393` are
  now C-pinned at the helper level but whole-task `g4` still
  diverges (`0xffffdffc` vs `0`) — the helper slice is the
  minimal proven step.

## 3. Tests

* New ROM-backed `vf2_coli_238a4_live` (+ `_differential`):
  loads `coli-parked-221e8`, steps to first/second `0x238a4`,
  compares `vf2_hybrid_coli_238a4_execute` vs reference stepping
  for both f0 and f1: `136`/`5` and `5`/`134`, full live-state
  equality.

* Existing `vf2_coli_238a4` warm still passes (5 steps).

## 4. Validation

* `ctest` 70/70 (was 68, +2).
* Clang ASan/UBSan green on `vf2_coli_238a4_live`,
  `vf2_coli_238a4_live_differential`.
* No ROM/snap/trace committed.

## 5. Next

Whole-task `0x221e8→0x10dcc` live `9385`/`9393` still needs the
`0x2396c`/`0x233d0` live `g4` and the `0x23524` FIFO `r11`/`r12`
divergence (`0xffffdffc`); the `0x238a4` live is the first
shell live piece.  After that, the whole-task `9529` (`9158` +
`371` midbody long) remains the explicit whole-tail next.
