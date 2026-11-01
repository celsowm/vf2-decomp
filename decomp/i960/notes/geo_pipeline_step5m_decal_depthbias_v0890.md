# geo_pipeline_step5m_decal_depthbias_v0890 — intra-object decal lift (stage-4 banner)

**Geometriser pipeline, Step 5 (decor), depth-bias.** kazuyette, in Blender on `stage_04`
after 5l: the back banner reads as a small panel of ~2 garbled glyphs ("卡央"-ish), not the
full-width **"决斗胜负"** of `stage_04b/screen.png`. An earlier pass had concluded the banner
"already bakes correctly" -- that was validated with a painter-order overwrite render (last
face drawn wins), which is **not** how Blender draws. Under a real z-buffer the sign vanishes.

## Diagnosis [MEASURED]

The banner glyph is a clamp-sampled atlas tile on `sheet0` (tex0 `x[1152,1216] y[256,512]`,
the right of two banner strips; the left strip `x[1088,1152]` is the unused "棋逢对手" tile),
mapped rotated across four faces of records `59f564`/`59f9c8` at `X[-4,4] Y[5.2,7.2] Z=16.5`.
Those glyph faces are **coplanar at exactly Z=16.5 with the brick window-header** (`tile1`,
`X[-8,8]`) **inside the same record**. On the arcade, painter order draws the glyph last (a
decal) so it wins; in a float z-buffer they z-fight, and the brick (drawn first, larger area)
eats the sign. `geo_stage_depthbias.py` (Step 5h) only separated coplanar faces of **distinct**
objects, so it never touched this intra-record decal.

A second trap: the two wall normals point **+Z, away from the −Z camera** (the geometriser
does not wind decor faces to a consistent side), so the historical "push the base along
−normal" is *toward* the viewer here, not away. And the two banner halves abut, so Pass A's
seam rule splits them unevenly. Measured occlusion (camera in front, nearer = smaller Z): a
naive base-push left `59f564` glyph at 16.38 (clear) but `59f9c8` glyph tied with its brick
at 16.44.

## Fix

Extend the depth-bias with an **intra-object decal lift**, in two phases:
- **Pass A — between objects:** unchanged (byte-identical to 5h/5l; verified by diffing the
  output with Pass B disabled against the committed build). Push a base back one layer per
  distinct front-er object whose in-plane box overlaps it.
- **Pass B — within an object:** a face that **AREA-overlaps** an earlier-drawn face of the
  same object (a true decal; merely-abutting tiles share only an edge and are skipped) is
  lifted **toward the origin** -- the arena interior, hence the viewer, since stages are
  feet-centred with decal walls facing in -- to just in front of the base's *committed* Pass-A
  position. Reading Pass A's result and walking the level in paint order makes the decal clear
  its base even when Pass A moved it, is symmetric for both banner halves, and stacks
  decal-on-decal correctly. Works for floors (viewer above) and ceilings (viewer below) too.

## Validation [MEASURED]

Full-scene z-buffer renders (nearer = smaller Z, matching `screen.png`): stage-4 **"决斗胜负"**
reads four characters left-to-right, gold frame on red, both halves clean, no z-fight; floor,
medallions, arches, backdrop intact. All 11 stages re-exported (single-frame + `_full`) with
no errors; spot-checked 0/1/4/7/8/10 -- tower alcoves, colosseum arches, and stage-8 floor
decals all sit correctly on their bases, nothing sunk or shimmering. Pass A output is
byte-identical to the committed build (only decal faces additionally move, by EPS). The change
is in the Python post-process only; no C change, and depth-bias is stage-only, so fighter OBJs
stay byte-identical. `geo_stage_depthbias.py` parses clean.
