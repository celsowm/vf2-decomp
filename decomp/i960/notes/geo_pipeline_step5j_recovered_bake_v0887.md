# geo_pipeline_step5j_recovered_bake_v0887 — recovered-object UV/bake: tile-boundary straddlers, not a th_ptr divergence

**Geometriser pipeline, Step 5 (decor), bake defect.** kazuyette: ROM-recovered decor in
`stage_NN_full` (the arena-pit floor, stage-4 `rec59afe0`/`rec59b2e0`) had texels that
baked blank/black. Task: instrument the texheader walk prim-by-prim on a recovered record
and fix the `ext_emit_piece` (UV) vs `ext_colour_sheet_piece` (bake) divergence; zero
unbaked texels on all 11 `_full`.

## Root cause [MEASURED] — not a th_ptr divergence

Instrumented both walks prim-by-prim (env `VF2_BAKE_DBG`). They are **identical**: same
`th_ptr`, same decoded rect per prim. So the earlier "emit vs bake th_ptr divergence"
hypothesis is wrong. The real cause is the **UV**: `offu/offv` bring a polygon's UV
*minimum* onto the tile `[0,size)`, but a polygon whose maximum still runs past one copy
of the texture samples a second copy the atlas does not hold, so that half bakes blank.
Two flavours, both missed by the old `span > 1.5x tile` tile trigger:
- **genuine multi-tile repeats** that straddle well past the tile (e.g. `rec59afe0` prim
  with UV-V `[256,768]` on a 256-tall cell = 3 copies) -- including **translucent** ones
  (the pit floor is `rend=3`), which Step 5i's `!tl_translu` guard wrongly kept on the
  atlas;
- polygons that merely **reach/straddle a tile boundary** (span < one tile, sitting across
  the seam), which no span threshold catches.

## Fix

1. **Tile when the offset-aligned UV runs past the tile**, not by a span ratio:
   `(mxu-offu) > wq+2` or `(mxv-offv) > hq+2`. Catches genuine straddlers (the big
   black areas) regardless of opacity -- the old `!tl_translu` exclusion is removed, and
   it is not needed: the arena sign never straddles (its UVs stay inside its cell), so it
   is still never tiled (no "three bands" regression).
2. **Clamp a non-tiled stage polygon's UV into the one baked copy**: `cu ≤ wq-0.5`,
   `cv ≤ hq-0.5`. A polygon that only reaches the far edge (texel `size`, which the bake --
   filling `0..size-1` -- leaves blank) now reads the last baked texel instead of the gap
   past it, a <=2-texel inset that is invisible. Genuine repeats took the tile path above
   and never reach this clamp.

Both are gated on stage decor (`tiles != NULL`); fighters pass NULL, so the UV path is
unchanged and character OBJ+MTL stay byte-identical (akira OBJ/MTL/tex0 verified).

## Verification [MEASURED]

A visited-sentinel build (cutout texels marked, so truly-unbaked RGB=0 is told apart from
legitimate translucent cutout RGB!=0) + per-vertex atlas/tile sampling at `floor(uv*size)`
(as a GPU samples; the first count used `uv*(size-1)` and over-reported boundary texels):
**0 truly-unbaked recovered-poly texels across all 11 `stage_NN_full`** (was ~200+).
Tile counts stay modest (s00..s10: 5 6 17 18 6 10 19 6 18 17 8; cap 64). gcc+clang clean
(`-Werror -Wconversion -Wshadow`); geo/display/geometry tests 5/5; all 11 re-exported.

## Note

The stage-4 arena **pit opening** still reads dark from above: that is the pit depth /
its floor geometry, a separate matter from the recovered objects' texels (which are now
fully baked) -- the recovered pit-edge structures (`rec59afe0` at Z=-16, Y~7, etc.) bake
and render textured.
