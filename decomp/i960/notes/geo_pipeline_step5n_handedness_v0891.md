# geo_pipeline_step5n_handedness_v0891 — left-handed Model 2 -> right-handed OBJ

**Geometriser pipeline, coordinate frame (whole export, stage AND fighter).** kazuyette, in
Blender after 5m: the stage-4 banner now renders complete, but the **entire scene is mirrored
left/right** vs `stage_04b/screen.png` at the same framing. Two independent proofs: (1) the
banner reads **"负胜斗决"** -- the four characters in reverse order, each glyph mirrored too;
(2) the Great Wall seen through the gate runs to the **right** in MAME but to the **left** in
Blender. So it is the geometry that is mirrored, not a texture. Capture:
`snap/blender_stage04_mirror.png`.

## Diagnosis [MEASURED]

Model 2's TGP world frame is **left-handed**; OBJ and Blender are **right-handed**. The export
copied world vertices verbatim, so a left-handed scene read as right-handed comes out reflected
across one axis -- here X (the horizontal). This is why a correctly-handed viewer showed the
banner mirrored: looking along +Z, +X is screen-left in a right-handed frame but screen-right
in the game's left-handed frame.

## Fix

In `ext_emit_poly` (the single low-level OBJ polygon writer, shared by every export -- stage,
`player1`, pieces, scene, and the main `geo-fighter`/`geo-stage`): **negate the X coordinate**
of each written vertex and **reverse the face winding** (emit the `f` indices back-to-front) so
the reflected geometry keeps its outward normals. UVs are unchanged. Applied once, it converts
the whole export to the right-handed frame.

## Why nothing downstream needed changing [MEASURED + reasoned]

kazuyette asked to re-check the winding orientation (5f/5g), the floor-normal rules, and the
depth-bias direction (5h/5m) for a hidden mirror compensation. None needed a change:
negate-X and reverse-winding are each orientation-reversing, so **together they preserve every
computed face normal** (det -1 x det -1 = +1). Every normal-based rule is therefore invariant --
the winding-to-`onrm` orientation, the floor-up rule, and depth-bias Pass A (-normal) all behave
identically; depth-bias Pass B (lift toward the origin) is position-based and self-corrects
around the fixed origin. Confirmed by re-rendering: floors stay right-side-up, decals sit on
their bases, and no new z-fighting appears.

## Validation [MEASURED]

At the game-camera framing with a z-buffer and a right-handed (Blender) viewer: the stage-4
banner reads **"决斗胜负"** left-to-right and the Great Wall runs to the right, matching
`screen.png`. All 11 stages re-exported (single-frame + `_full`), no errors; stages 0/1/4/7/8/10
spot-checked clean. Fighters: re-exported akira is **exactly the X-mirror of the pre-change
export** (vertex multiset identical after negating X; 4883 verts / 1295 faces unchanged), and
rendered right-handed it matches MAME -- Akira faces right with the headband tails trailing
left (`snap/colour_akira_dural/screen.png`). The fighter OBJs therefore change (they were
mirrored before); the earlier "byte-identical" bar is intentionally superseded for this frame
fix. CI build clean (`-Werror -Wconversion -Wshadow`); geo/display/geometry tests 5/5.
