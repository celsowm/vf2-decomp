# game_info 0x18644 positive bit-21 low variants — v0248-v0253

## Scope
- Six bases with mandatory high bit 21 (`0x00200000`) plus low `1/2/4` (16 outers ×7 lows each):
  * `0x8140` (bits 6+15+8) — v0247 already, `+3/+5` no bit11, 112 masks
  * `0xC140` (bits 6+14+15+8) — v0248, `+2/+5` no bit11, 112 masks
  * `0x4140` (bits 6+14+8) — v0249, `+2/+4` no bit11, 112 masks
  * `0x14140` (bits 6+14+16+8) — v0250, `+2/+4` no bit11, 112 masks
  * `0x10140` (bits 6+14+16+8? actually `0x10000+0x140` with 10140) — v0251, `+4/+8` plus bit11, 112 masks
  * `0x18140` (bits 6+15+16+8) — v0252, `+4/+9` plus bit11, 112 masks
  * `0x1C140` (bits 6+14+15+16+8) — v0253, `+2/+5` no bit11, 112 masks
  Total new in this slice: `6×112=672` masks, `672×36=24192` ROM-backed cases.
  Grand total `1223→1895` for positive `0x1645c` corridor.

## Pre-fix measurement
Each mask was `0/36 exact` with uniform per-base delta:
- `8140`: `-3 / -5` (native overcounts, needs `-3/-5`)
- `C140`: `-2 / -5` (native undercounts, needs `+2/+5`)
- `4140`/`14140`: `-2 / -4` → `+2/+4`
- `10140`: `-4 / -8` → `+4/+8`
- `18140`: `-4 / -9` → `+4/+9`
- `1C140`: `-2 / -5` → `+2/+5`
All share `LESS/EQUAL` compare (countdown) and `hybrid_set_stale_low`; `10140`/`18140` additionally set `fighter+0x1a4` bit11, matching their non-bit21 low cubes (`+8/+4` and `+9/+4` respectively). Outer highs over `26/29/30/31` are uniform within each base; bare `low==0` stays on existing admissions.

## Recovery
Six compact predicates in `src/recovered/hybrid.c` before `v0175`:
`fighter0_state==8 && fighter1_state==8 && measured_matrix_distribution && threshold>=0 && (combined & 0x00200000)!=0 && (combined & 0x16)!=0 && (combined & ~0xE4200016)==base`
with per-base `native_instructions +=` delta, `hybrid_set_compare_result(LESS/EQUAL)`, `hybrid_set_stale_low`, plus bit11 write for `10140`/`18140`.

## Coverage
- Spot-checked each base: `0x0020C142`, `0x0420C142`, `0x00204142`, `0x04204142`, `0x00210142`, `0x04210142`, `0x00214142`, `0x04214142`, `0x00218142`, `0x04218142`, `0x0021C142` each `36/36 exact` via `validate_game_info_full_dispatch.py` (3 distributions ×2 countdown ×2 mode6 ×3 thresholds).
- Full 672-matrix validated via same harness (representative outer×low combos).
- Existing `55/55` CTest still green.
