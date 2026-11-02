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
pixel`. Palette entry 5-5-5 (R bits 0-4, G 5-9, B 10-14). Scroll: hscroll word `0x5000+t`,
vscroll/disable `0x5004+t`.

## All 11 stages [MEASURED]

The plane layout is **identical across all 11 stages** (`bg_stage_00` .. `bg_stage_10`):
- plane 0 (0x0000) = **HUD** (green life bars, TIME, AKIRA/<opponent>, WIN) — ~12k opaque
  pixels, all in the top band, ~0.47 green — dropped.
- plane 1 (0x1000) = empty.
- plane 2 (0x2000) = the **complete visible backdrop** (sky + mountains/scenery), fully opaque
  across the whole 512x512.
- plane 3 (0x3000) = a solid sky-colour fill behind plane 2 (hidden, since plane 2 is opaque
  everywhere; it is the fallback/base colour).

Background = tilemap 1; HUD = tilemap 0; scroll (0,0) everywhere. Because plane 2 is fully
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

`--obj` also writes `bg_plane.obj` + `bg_plane.mtl` next to `<out.png>`: a single flat textured
quad carrying the backdrop PNG, ready to import behind the decor. It is written in the **same
frame as the stage OBJ export** (feet-centred, +X -> screen-left, looking +Z -- the
ext_emit_poly convention of a negated X and reversed winding), so it drops into Blender already
aligned. The quad sits at a fixed depth behind the decor -- the farthest Z of a `stage.obj`
sitting next to `<out.png>` (times 1.3 + 30), else `--depth Z`, else 400 -- and is sized from
the placement camera (the game-view approximation that reproduces `screen.png` from the stage
export: `cx,cy,cz = 0,6,-11`, focal `scale=430` over a 640x480 raster) so the on-screen 496x384
tilemap window exactly fills the frame, the full 512x512 texture extending just past the edges.
Flat, no parallax; render with a z-buffer (not painter order) so the decor occludes it.
Validated on stage 4: a game-camera z-buffer render of `stage.obj` + `bg_plane.obj` shows the
green mountains through the gate and above the wall, the wall correctly in front, matching
`stage_04/screen.png`. The depth auto-adapts per stage (e.g. `stage_10` 137.8 vs
`stage_10_full` 281.4). bg_plane files are snapshots too, never committed.

`--blender` writes `blender_scene.py` next to `<out.png>`: the stage OBJ carries no camera, so
in Blender's free viewport the backdrop looks wrong and (lit) washed out. The script restores
the game view. Run it in the Scripting tab (open from disk, Run) or headless
(`blender -b -P blender_scene.py -f 1`). It imports stage.obj + bg_plane.obj **forward -Z /
up Y** (the manual-import axes), builds camera **VF2_GameCam**, sets 496x384 + Standard view
transform, and rebuilds every material as **unlit Emission** (texture Color -> Emission, texture
Alpha -> transparency mix) so the baked stage textures and the flat backdrop show as-is.

**Axis mapping [MEASURED in Blender 5.2].** The forward -Z / up Y OBJ import maps OBJ `(x,y,z)`
-> Blender `(x, -z, y)` (NOT `(-x, z, y)`). So the placement camera -- OBJ `(0,6,-11)` looking
+Z, up +Y -- becomes in Blender: location `(0, 11, 6)`, **looking -Y, up +Z** (rotation built
from an explicit basis matrix, not guessed Euler angles); OBJ +X stays screen-left. Getting the
sign wrong points the camera away from the decor (only the near floor renders, the wall and
backdrop fall behind the camera -> black). Lens from `scale=430` over the **640-px reference**
raster (render_persp_bl's raster), sensor 36 mm -> 24.19 mm. This reproduces screen.png's content
(mountains + white tower through the gate, wall in front); it is a touch wider than the in-game
crop -- the game camera is tighter than scale=430 -- so fine-tune VF2_GameCam's lens/position in
Blender for an exact crop. EEVEE's depth test occludes the backdrop correctly (z-buffer, not
painter). The generated .py and any render it writes are local snapshots, never committed.

## Status

All 11 stages captured (`snap/bg_stage_00` .. `bg_stage_10`), decoded and validated against
their `screen.png`. The HUD/background split (HUD = tilemap 0, backdrop = tilemap 1) held for
every stage; one paused frame per stage was enough (no tile streaming observed). The
`readstart` in each `info.txt` (0x803008) is the fighter colour-data offset, not the tilemap,
so the tool does not use it. Note `tilemap_bg.py` writes PNG snapshots only (never committed);
the fighter OBJs are untouched (Python-only), so the geometry stays byte-identical.
