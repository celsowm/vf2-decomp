# tilemap_2d_background_v0892 — recover the 2D sky/mountain backdrop

**New layer: the Model 2 2D tilemap.** The sky and distant mountains behind every VF2 stage
are NOT 3D decor -- the 3D export (geo-stage) draws black where they are. They are the Model 2
**Sega System-24-style tilemap** (segas24_tile): four 8x8 4bpp scroll planes over one palette.
VF2 draws the **HUD** (life bars, timer, names, WIN) on one tilemap and the **backdrop** on the
other, in the same VRAM.

## Capture [kazuyette]

The colour captures did not include the tilemap RAM (only its palette). kazuyette added two
regions to `snap/bg_stage_04/` (i960 program-space words, same dump tool as the colour
captures):
- `tileram_01000000.bin` 0x10000 — the four plane nametables + scroll/control (i960 0x01000000)
- `charram_01080000.bin` 0x80000 — 8x8 4bpp tile pixels (i960 0x01080000)

The 2D palette was already present: low 0x1000 words of `palette_01800000.bin`.

## Format [MEASURED, from MAME segas24_tile -- layout, clean-room]

Tile RAM = 0x8000 words. Plane `p` nametable = words `[p*0x1000 .. +0xfff]`, 64x64 cells of
8x8. Planes 0/1 = tilemap 0, planes 2/3 = tilemap 1. Nametable word: char# = `val & 0x3fff`,
colour bank = `(val>>7)&0xff`, bit 15 = priority. Char: 32 bytes/tile, 4 bytes/row, each byte
two pixels (low nibble even X, high odd X); pixel 0 transparent; palette index = `colour*16 +
pixel`. Palette entry 5-5-5 (R bits 0-4, G 5-9, B 10-14), but it is **not expanded bit-for-bit**:
each 5-bit channel goes through the **colorxlat LUT** (model2.cpp palette handler: word index
`base + (v<<8)`, base 0x40 R / 0x2040 G / 0x4040 B, low byte) and then the **fixed VF2 screen DAC
gamma** `max((v-64)*255/191, 0)` -- the same path the 3D pipeline uses. A raw `5-bit<<3` came out
~1/0.74 too bright (MEASURED: screen/bg sky ratio 0.75/0.74/0.74 with `<<3`, 1.02/1.00/0.99 with
colorxlat+gamma). **Scroll is per PLANE** (not per
tilemap): hscroll word `0x5000+plane`, vscroll word `0x5004+plane`, each bits 0-8; hscroll bit 15
= per-row scroll enabled. Screen pixel (sx,sy) shows tilemap pixel (sx+hscroll, sy+vscroll). The
backdrop plane is plane 2 (0x2000). [MEASURED] vscroll per stage (plane 2): 00=0, 01=24, 02=0,
03=40, 04=0, 05=24, 06=96, 07=24, 08=0, 09=24, 10=32; hscroll 0 everywhere; per-row scroll on
07/09/10. (The earlier "scroll (0,0) everywhere" was wrong -- it read the plane-1 words, which are
empty; stage 3's backdrop was 40 px too high.)

## All 11 stages [MEASURED]

The plane layout is **identical across all 11 stages** (`bg_stage_00` .. `bg_stage_10`):
- plane 0 (0x0000) = **HUD** (green life bars, TIME, AKIRA/<opponent>, WIN) — ~12k opaque
  pixels, all in the top band, ~0.47 green — dropped.
- plane 1 (0x1000) = empty.
- plane 2 (0x2000) = the **complete visible backdrop** (sky + mountains/scenery), fully opaque
  across the whole 512x512.
- plane 3 (0x3000) = a solid sky-colour fill behind plane 2 (hidden, since plane 2 is opaque
  everywhere; it is the fallback/base colour).

Background = tilemap 1; HUD = tilemap 0; scroll per plane (above). Because plane 2 is fully
opaque, the composited backdrop is effectively plane 2. No stage needed a `--bg-tilemap`
override. Each decoded backdrop was compared to its own `screen.png` and matches:

| # | owner / stage | backdrop |
|---|---------------|----------|
| 00 | Lion — stone towers | violet sky gradient + grey mountains (seen between the towers) |
| 01 | Shun | dark crimson sunset sky + dark hills |
| 02 | Sarah | misty grey-white sky + green forested mountain over a lake |
| 03 | Lau — temple | blue sky with wispy clouds, low distant hills |
| 04 | Kage — Great Wall | green mountains + Great Wall, through the gate window |
| 05 | Wolf — beach | blue sky with large cumulus clouds |
| 06 | Pai | purple-to-orange sunset gradient |
| 07 | Jacky | vivid orange/magenta sunset + distant sea horizon |
| 08 | Akira — snow ring | snowy mountain peaks + blue sky (behind the chain fence) |
| 09 | Lion — lion statue | blue sky with scattered clouds |
| 10 | Lau — Dural underwater | blue underwater haze + sunken Parthenon ruin |

Full image is 512x512; the sky/scenery sits above the horizon, the band below is covered
in-game by the 3D arena (kept in the PNG so Blender has the full layer; crop at the camera).

## Tool

`tools/python/tilemap_bg.py <capture-dir> <out.png>` decodes the planes, drops the HUD tilemap
and writes the backdrop PNG, printing the scroll and a placement note. `--bg-tilemap /
--hud-tilemap / --wfront` override the plane assignment if a stage differs; `--dump-layers`
writes each plane for inspection. It is a flat 2D backdrop (no parallax): in Blender place the
PNG on a large flat plane (or shallow cylinder) square behind the decor, filling the camera
frame; the on-screen window is 496x384 at the reported scroll. Python-only, fighters untouched
(byte-identical). Output PNGs are snapshots, never committed.

## The game camera (not eyeballed) -- `geo-stage` emits `camera.txt`

The stage OBJ is exported *camera-removed* (feet-centred), so Blender has no game camera and a
free-viewport view of the backdrop is wrong. The camera must be the **exact view transform the
export removed**, not one calibrated by eye. `geo-stage` now writes **`camera.txt`** beside
`stage.obj`: the shared decor matrix `mdec` (A, T) that `gs_worldframe` inverts, plus the Model 2
projection. A captured decor vertex `v` (camera space) and an OBJ vertex `o` are related by
`v = fc.A + T`, `fc = (-o.x, o.y, o.z)` (the OBJ negates X); the projection (MEASURED, read from
the capture by `geo-project`, same for every stage: focus 600/600, window-centre 248/319,
xoff 0, yoff 130) is `sx = 248 + 600*vx/vz`, `sy = 195 - 600*vy/vz` over 496x384. Stage 4's A is
~identity with a ~4 deg X pitch, T ~= (0, -1.16, 4.95).

**Ground truth.** `geo-project <ram9> <readstarts> <rom-dir> <pose> out.png 496 384 0 130`
projects the frame with this exact transform (it tracks the matrix/focal/window itself). A
software reprojection of `stage.obj` through `camera.txt` lands the banner within ~3 px of
`stage_04/screen.png` -- the floor is set by the reconstructed decor's own fidelity, not the
camera.

## `--obj` / `--blender` use `camera.txt` (run geo-stage on the same capture first)

`--obj` writes `bg_plane.obj` + `bg_plane.mtl`: a flat textured quad built in the camera's VIEW
space -- a plane at a depth just behind the decor (`decor_max_vz * 1.1 + 5`, or `--depth`), sized
so the 496x384 on-screen tilemap window exactly fills the game frame, the full 512x512 texture
extending just past the edges -- then mapped to OBJ coords (`fc = (view-T).A^-1`,
`OBJ = (-fc.x, fc.y, fc.z)`). It imports aligned with `stage.obj` and shows ONLY through the
decor's openings. Because the camera pitches, the quad is tilted in OBJ space (that is correct).

`--blender` writes `blender_scene.py`: imports stage.obj + bg_plane.obj **forward -Z / up Y**,
builds **VF2_GameCam** from `camera.txt`, sets 496x384 + Standard, and makes every material
**unlit Emission** (so baked textures + backdrop show as-is, not washed out). It prefers
**Cycles** (ray-traced: the coplanar wall decals -- the banner -- and the backdrop-through-opening
resolve by true depth); EEVEE's rasteriser mis-orders the coplanar decals and is a fast fallback.
The backdrop material writes depth (so the decor z-occludes it); the decor + decals stay painter-
ordered (BLEND) like MAME so the banner shows on the wall.

**Folder resolution.** `_here()` tries each candidate -- `__file__`'s dir (blender -P), the open
text's dir (Text Editor Run), the .blend dir, cwd -- and returns the first that actually contains
`stage.obj`; a baked-in absolute `_GEN_DIR` is the final fallback. This matters because the
Scripting tab can set `__file__ = "\blender_scene.py"` (dir `<drive>:\`, no stage.obj), so the
first candidate must be rejected, not trusted. The non-existent `bpy.ops.import_scene.obj`
fallback is gone (it only masked the real error on 5.x). Verified by execing the script with
`__file__="\blender_scene.py"` from a different cwd: it still resolves to its own folder.

**Decor weld (hairline cracks).** The export writes each polygon's vertices independently, and
adjacent wall polygons -- within a record and across records -- can carry the same corner at
coordinates differing by fixed-point rounding, leaving sub-pixel gaps that let the backdrop show
as thin lavender lines (MAME rasterises at whole pixels and never shows them). Measured on stage 4:
crack pairs sit at **<= 0.106 u** (both within and across records), the smallest real feature at
**0.37 u** -- a clean gap. The script joins the stage pieces (NOT bg_plane) into `VF2_Stage` and
**Merge-by-Distance at 0.15 u** (`MERGE_DIST`), closing every crack and touching no feature; decals
are separate quads with no near-duplicate of the wall, so they are not merged (the banner stays
intact). After the weld the wall carries no backdrop pixels (only the legitimate sky sliver above
the wall top, which the HUD covers in game).

**Axis / camera conversion [MEASURED, Blender 5.2].** The forward -Z / up Y import maps OBJ
`(x,y,z) -> Blender (x, -z, y)`. `blender_camera()` turns `camera.txt` into a Blender pose: the
linear map `view = L.P_blender` (from `fc=(-bx,bz,-by)`, `view=fc.A+T`) is inverted so the camera
local axes are L^-1's columns (right = view x, up = view y, forward = -view z); the optical centre
is `fc0=-T.A^-1` mapped to Blender. Intrinsics: `lens = fx*sensor/W` (600*36/496 = 43.55 mm),
`shift = ((px,py) - centre)/W` (sign verified against the exact reprojection). Set `matrix_world`
directly, not guessed Euler angles.

**Validation [MEASURED] -- stage 4.** Headless Cycles render from VF2_GameCam vs
`stage_04/screen.png`: banner gold-plaque corners all at **(+0, +3) px** (horizontal exact,
vertical +3 = the decor-fidelity floor, matching the exact software reprojection); the mountains
through the opening align full-width, vertical within 3 px; the wall is correctly in front and
the backdrop shows only through the opening. 50% overlay `overlay_04.png`. The arena floor the
fighters stand on is present in plain `stage.obj` (not only `_full`). `__file__` resolution also
handles the Text-editor Run (open text's filepath) and the .blend dir.

**Per-stage re-export.** Each stage is re-exported from its OWN `bg_stage_NN` capture (geo-stage
with that frame's readstart) so `stage.obj`, the textures and `camera.txt` all come from the frame
`screen.png` was shot from. A stage left on a different capture's textures looked wrong (stage 3
had an orange roof, beige walls, a green doorway) -- re-exporting from `bg_stage_03` restored the
gold roof, red lattice, white dragon panels, blue floor and temple doors. The decor world frame is
capture-independent (the camera cancels in `decor_model . mdec . mdec^-1`), so the geometry stays
consistent; only the per-capture `camera.txt` and textures change.

**Validation [MEASURED] -- all 11, NCC of the Cycles render vs `screen.png`, HUD + fighters masked.**
Best-shift `(dx, dy)` and NCC:

| stage | dx | dy | NCC | | stage | dx | dy | NCC |
|---|---|---|---|---|---|---|---|---|
| 00 | 0 | -3 | 0.72 | | 06 | 0 | -3 | 0.83 |
| 01 | 0 | -3 | 0.72 | | 07 | 0 | -3 | 0.77 |
| 02 | 0 | -3 | 0.35 | | 08 | 0 | -3 | 0.54 |
| 03 | 0 | -3 | 0.81 | | 09 | 0 | -3 | 0.65 |
| 04 | 0 | -3 | 0.79 | | 10 | 0 | -3 | 0.41 |
| 05 | 0 | -2 | 0.43 | | | | | |

`dx = 0` on every stage and `dy` does NOT vary per stage (it would, by the vscroll, if the scroll
were still wrong) -- so the per-plane scroll is correct. The residual uniform `dy = -3` is the
decor-reconstruction fidelity floor (the exact software reprojection shows the same +3). The lower
NCCs (02/05/10) are water / sand / underwater scenes: large soft-textured areas plus the fighters'
floor shadows (present only in `screen.png`) correlate weakly, though they match visually. Contact
sheet `contact_sheet.png` (render | screen, masked regions dimmed).

## Status

All 11 stages captured (`snap/bg_stage_00` .. `bg_stage_10`), decoded and validated against
their `screen.png`. The HUD/background split (HUD = tilemap 0, backdrop = tilemap 1) held for
every stage; one paused frame per stage was enough (no tile streaming observed). The
`readstart` in each `info.txt` (0x803008) is the fighter colour-data offset, read per stage for
the re-export. `tilemap_bg.py` writes PNG/OBJ snapshots only (never committed); the fighter OBJs
are untouched (Python-only).
