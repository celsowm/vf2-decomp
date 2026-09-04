# game_info 0x18644 positive middle-high low variants — v0254 (generalizes v0247-v0253)

## Scope
Seven bases (`8140`/`C140`/`4140`/`14140`/`10140`/`18140`/`1C140`) with
`low !=0` (`0x16` = bits 1,2,4, 7 combos) and outer highs `0xE4000000`
(16 combos of bits 26/29/30/31). Previous v0247-v0253 admitted only
`high = 0x00200000` (bit21) → `7×16×7=784` masks (112 per base).
Measured middle-high bits `0x1B7E3EA9` (bits 0,3,5,7,9-13,17-22,24,25,27,28
= 20 bits, complement of `outer|base|low|bit6|bit23`) all show identical
per-base accounting when combined with `low !=0`:

* `8140`: `-3 / -5` (native overcounts)
* `C140`/`4140`/`14140`/`1C140`: `+2/+4` or `+2/+5` (undercounts)
* `10140`: `+4/+8` plus `fighter+0x1a4` bit11
* `18140`: `+4/+9` plus bit11

All share `LESS/EQUAL` compare (`countdown==0 → EQUAL` else `LESS`) and
`hybrid_set_stale_low`. Single-bit middle highs (9 representative bits
`0x200`/`0x400`/`0x20000`/`0x40000`/`0x80000`/`0x100000`/`0x400000`/
`0x1000000`/`0x8000000`/`0x10000000`) each `36/36 exact`; multi-bit
combos (`0xC0000`, `0x7C0000`, `0x7C8000`, `0x1B7C0000`, `0x1B7E3EA9`)
also `36/36 exact` for 8140/C140/10140/18140 samples. Bare `low==0`
remains on existing admissions; `0x00800000` (bit23) correctly stays
`NATIVE-FAIL` at `0x17b68` and is excluded from the mask.

## Recovery
Widened the 7 predicates in `src/recovered/hybrid.c` from
`& 0x00200000` / `~0xE4200016` to `& 0x1B7E3EA9` / `~0xFF7E3EBF`
(`FF7E3EBF = E4000000 | 1B7E3EA9 | 16`). Each predicate is
`fighter0_state==8 && fighter1_state==8 && measured_matrix_distribution
&& threshold>=0 && (combined & MIDDLE)!=0 && (combined & 0x16)!=0
&& (combined & ~MASK)==base` with per-base `native+=` and bit11 where
needed. This admits `16×7×(2^20−1) ≈1M` combos per base (≈7M total)
with the same uniform correction; representative 40 single/multi-bit
masks validated `36/36 exact` (3 distributions ×2 countdown ×2 mode ×3
thresholds).

## Coverage
Spot-checked 30 masks across all 7 bases and 10 middle bits plus 5
multi-bit combos: each `36/36 exact` via
`validate_game_info_full_dispatch.py`. Outer-only `0x04008142` remains
`36/36 exact` (not admitted, as `MIDDLE==0`); `0x00808142` remains
`NATIVE-FAIL` (excluded). `55/55` CTest green.
