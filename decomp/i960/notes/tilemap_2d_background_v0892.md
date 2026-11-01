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

## Stage 4 [MEASURED]

Decoding the four planes of `bg_stage_04`:
- plane 0 (0x0000) = **HUD** (green life bars, TIME, AKIRA/KAGE, WIN) — dropped.
- plane 1 (0x1000) = empty.
- plane 2 (0x2000) = **mountains + sky gradient** (the Great Wall landscape), opaque.
- plane 3 (0x3000) = solid sky-blue fill, behind.

Background = tilemap 1 (planes 2+3, 's' plane composited over 'w'); HUD = tilemap 0. Scroll
(0,0). The result matches `stage_04b/screen.png`: the same green mountains, Great Wall
structure and sky seen through the gate window. Full image is 512x512; the sky/mountains sit
above the horizon, the band below is covered in-game by the 3D arena.

## Tool

`tools/python/tilemap_bg.py <capture-dir> <out.png>` decodes the planes, drops the HUD tilemap
and writes the backdrop PNG, printing the scroll and a placement note. `--bg-tilemap /
--hud-tilemap / --wfront` override the plane assignment if a stage differs; `--dump-layers`
writes each plane for inspection. It is a flat 2D backdrop (no parallax): in Blender place the
PNG on a large flat plane (or shallow cylinder) square behind the decor, filling the camera
frame; the on-screen window is 496x384 at the reported scroll. Python-only, fighters untouched
(byte-identical). Output PNGs are snapshots, never committed.

## Next

Stage 4 confirmed. The other 10 stages need the same two capture regions (kazuyette to dump).
The HUD/background tilemap split (HUD = tilemap 0) is assumed constant; `--bg-tilemap` covers a
stage that differs. One paused frame holds the whole nametable, so a single capture per stage
is enough unless a stage streams tiles.
