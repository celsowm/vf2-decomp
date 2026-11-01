# geo_pipeline_step5l_arena_center_floor_v0889 — the arena-centre floor the fighters stand on

**Geometriser pipeline, Step 5 (decor), missing object.** kazuyette, in Blender: `a9bea0`
(the stage-4 arena floor) is a frame with a **square hole** in the middle and the Blender
grid shows through it, but in `stage_04b/screen.png` the fighters stand on stone there.
The central arena floor object was not being exported -- same on the other stages.

## Diagnosis [MEASURED]

Dumped every display-list object for `stage_04b` with its 3x3-determinant flatness, its
distance to the shared camera/decor matrix `mdec`, and its translation. The decor kept by
geo-stage (`rec 599e64..5a03a8`, `a9bea0` among them) all carry `mdec` exactly. The arena
centre floor is **`rec 12ce78`** (forced-in footprint `X[-8,8] Z[-8,8]` at floor level,
exactly a9bea0's hole): it sits at the **same decor translation** `T=(0,-1.2,4.9)` but
with its **own 3x3**, so the geo-stage decor test (full matrix within `1e-3`) dropped it.
Each fighter limb and its shadow sit at the fighters' own translations, so translation
alone separates decor from fighters. The stone-towers stage (0) is worse: its frame
`rec 581d50` and centre pad `rec 1a9b54` are in the hard "bad" blacklist -- there for the
fighter export, but they ARE the stage-0 arena floor.

## Fix

In `command_geo_fighter`'s geo-stage decor filter:
- keep an object that shares the decor **translation** (|ΔT| < 0.05), not only one that
  matches the full matrix -- recovers the centre floor (`12ce78` and its per-stage
  equivalents) without pulling in any fighter/shadow (they sit elsewhere);
- gate the `581d50`/`1a9b54` blacklist on `!stage_mode`, so the fighter export is unchanged
  but geo-stage keeps the stone-towers floor. (Both are still barred from the `mdec` camera
  search, so the camera detection is unchanged.)

## Validation [MEASURED]

All 11 single-frame stages now have an object covering the arena centre (|XZ|<4, floor
level): s00 581d50+1a9b54, s01 …+57da90, s02 001584+003624, s03 57fd08+1a2910, s04
a9bea0+**12ce78**, s05 …, s06 …, s07 a9a474+11ec40, s08 0523fc+19b4ec, s09 11bb68+17e81c,
s10 … . Object counts rose only +1..+2 (stage-4 26→27), no junk. `12ce78` is textured
(tile6/sheet1/tile7). Fighter export byte-identical (akira OBJ+tex0 -- the blacklist and
the translation rule are both stage-only). gcc+clang clean (`-Werror -Wconversion
-Wshadow`); geo/display/geometry tests 5/5; all 11 re-exported (single-frame + `_full`).
