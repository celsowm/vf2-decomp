# game_info 0x18644 positive middle-high bare+low — v0255 (extends v0254)

## Scope
Extends v0254's middle-high set `0x1B7E3EA9` (20 bits) from `low !=0`
to `low any` (8 combos including bare `0x00`). Outer remains `16`
combos. Seven bases as before. Spot-checked bare middle-high masks
(`0x00048140`, `0x0004C140`, `0x00044140`, `0x00054140`, `0x00050140`,
`0x00058140`, `0x0005C140`, `0x04048140`) each `36/36 exact` after
removing the `low !=0` guard; outer-only bare (`0x00008140`,
`0x04008140`) stays exact (middle==0), `0x00800000` stays `NATIVE-FAIL`.

## Recovery
Removed `&& (combined & 0x16)!=0` from the 7 predicates introduced in
v0247-v0254, leaving `fighter0_state==8 && fighter1_state==8 &&
measured_matrix_distribution && threshold>=0 && (combined & 0x1B7E3EA9)!=0
&& (combined & ~0xFF7E3EBF)==base` plus per-base `native+=` and bit11.
This admits `16×8×(2^20−1) ≈8M` combos per base (bare+low) with same
uniform `−3/−5`/`+2/+5`/`+4/+8`/`+4/+9`, `LESS/EQUAL` and `stale_low`.

## Coverage
* 8 bare middle-high masks above `36/36 exact` via
  `validate_game_info_full_dispatch.py` (3 dists ×2 cd ×2 mode ×3 thresholds)
* Previous low variants remain `36/36 exact`
* `55/55` CTest green
