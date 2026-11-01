# geo_pipeline_step5k_wrap_mirror_v0888 — revert the mirror flip; wrap-aware tiling (sign shear)

**Geometriser pipeline, Step 5 (decor).** kazuyette, in Blender on `stage_04` after 5i:
(1) the front banner "棋逢对手" renders its left half "棋逢" correctly but the right half
"对手" as a slanted red shard; (2) the yellow bands and the back banner "决斗胜负" read
mirrored; (3) the central slab is uniform light grey with no stone pattern and the front
wall has a V cut-out. Reference: `stage_04b/screen.png`, both texts read left-to-right.

## (2) Mirror flip reverted [FIXED]

Step 5i flipped a polygon's UV within the tile when the texheader `mirror_x`/`mirror_y`
(th0 bit 8/9) was set, on the theory the hardware samples the tile flipped. That is wrong:
those bits select a **wrap MODE** (mirror-repeat vs repeat **past the tile edge**), not a
whole-texture flip, and the validated MAME scene render ignores them -- so the atlas/UV
path must too. The flip mirrored every sign that set the bit (stages 0/3/5/7/8/9; stage-4
has **0** mirror-bit polys, so its mirrored look is not from this flip -- see below).
Reverted the flip and its counter.

## (1) Wrap-aware tiling [FIXED for the shear; a residual remains]

The repeat-tile path (Step 5d) emitted a straddling polygon's UV in tile units on **both**
axes, i.e. REPEAT on both. But a decal/sign is a **linear** texture that repeats in at most
one axis; its `wrap_x`/`wrap_y` bits (th0 6/7) say which. Of stage-4's tiled polys, 148 are
`wx=0 wy=1`, 14 `wx=1 wy=0`, 58 `wx=1 wy=1` -- so **most** have a non-wrapping axis that the
tile path was wrongly repeating, shearing the banner text. Fix: tile an axis only where its
wrap bit is set **and** its UV runs past one copy; a non-wrapping axis that overruns is
clamped into the one baked copy (`tcu ≤ wq-0.5`) instead of repeated. The bake stays fully
covered (0 truly-unbaked on stage_04_full preserved), fighters byte-identical (akira
OBJ/MTL/tex0), gcc+clang clean, geo/display/geometry tests 5/5.

## Residuals (open)

- **Large linear signs** (`wx=0`, U wider than the 256 texheader tile -- the front banner's
  "对手" half): the atlas bakes only the texheader's one tile, so the continuation past it is
  not baked; wrap-aware clamping now shows the first tile cleanly instead of a false repeat,
  but the second half still is not present. Correct fix needs **bake-extend**: bake the
  polygon's full UV span from texture memory for a linear axis (watching for atlas overlap),
  then sample it directly. Deferred.
- **Central slab uniform grey (3):** `tile3` is a correct dark stone pattern (mean 76,
  stdev 28) and a9bea0 tiles it; the flat grey is likely the Blender material's wrap
  Extension (must be Repeat) or the 12x density aliasing, not a bad texture -- needs a
  kazuyette Blender check. The front-wall "V cut-out" is to be confirmed (possibly the same
  linear-sign bake gap, or missing pit-wall geometry).
