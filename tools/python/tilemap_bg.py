"""Export a VF2 stage's 2D background (sky + mountains) from a Model 2 tilemap capture.

The Model 2 2D layer is a Sega System-24-style tilemap (segas24_tile): four 8x8 4bpp
scroll planes sharing one palette. VF2 puts the HUD (life bars, timer, names, WIN) on
one tilemap and the sky/mountain backdrop on the other, so the background is recovered by
decoding the backdrop tilemap and dropping the HUD tilemap.

Capture layout (one paused frame, i960 program-space words, same dump tool as the colour
captures):
  tileram_01000000.bin  0x10000  nametables (4 planes) + scroll/control, at i960 0x01000000
  charram_01080000.bin  0x80000  8x8 4bpp tile pixels, at i960 0x01080000
  palette_01800000.bin  0x04000  palette RAM; the low 0x1000 words are the 2D palette

Tilemap format (from MAME segas24_tile, clean-room -- layout, not code):
  - tile RAM is 0x8000 words. Plane p nametable = words [p*0x1000 .. p*0x1000+0xfff],
    64x64 cells of 8x8. Planes 0,1 are tilemap 0; planes 2,3 are tilemap 1.
  - nametable word: char#   = val & TILE_MASK (0x3fff, the char RAM holds 0x4000 tiles)
                    colour  = (val >> 7) & 0xff   (16-colour bank)
                    bit 15  = per-tile priority category
  - char: 32 bytes/tile, row = 4 bytes, each byte two pixels (low nibble = even X, high =
    odd X); 4bpp. palette index = colour*16 + pixel; pixel 0 is transparent.
  - palette entry (low 0x1000 words): 5-5-5, R in bits 0-4, G 5-9, B 10-14.
  - scroll: hscroll word 0x5000+t, vscroll/disable word 0x5004+t (t = tilemap index);
    value is bits 0-8, vscroll bit 15 disables the tilemap.

HUD vs background: VF2 draws the HUD on tilemap 0 (planes 0x0000/0x1000) and the backdrop
on tilemap 1 (planes 0x2000/0x3000). `--bg-tilemap`/`--hud-tilemap` override if a stage
differs. Within the background tilemap the 's' plane (lower offset) is composited in front
of the 'w' plane; `--wfront` swaps that.

Usage:
  python tilemap_bg.py <capture-dir> <out.png> [--bg-tilemap 1] [--hud-tilemap 0] [--wfront]
                                               [--dump-layers <dir>]

The PNG is the full 512x512 backdrop panorama (sky + mountains above the horizon; the band
below the horizon is covered in-game by the 3D arena). It is a FLAT 2D layer, not a 3D sky,
so place it on a large flat plane (or a shallow cylinder) square behind the decor, filling
the camera frame; the on-screen window is 496x384 at the reported scroll. See the printed
note. Local tool; the PNGs it writes are snapshots and are never committed.
"""
import sys, os, struct

TILE_MASK = 0x3fff
SCREEN_W, SCREEN_H = 496, 384


def _u16(path, nwords):
    return struct.unpack("<%dH" % nwords, open(path, "rb").read())


def load_capture(cap):
    tile = _u16(os.path.join(cap, "tileram_01000000.bin"), 0x10000 // 2)
    char = open(os.path.join(cap, "charram_01080000.bin"), "rb").read()
    palw = _u16(os.path.join(cap, "palette_01800000.bin"), 0x4000 // 2)
    return tile, char, palw


def pal_rgb(palw, idx):
    v = palw[idx & 0xfff] & 0x7fff
    return ((v & 31) << 3, ((v >> 5) & 31) << 3, ((v >> 10) & 31) << 3)


def decode_plane(tile, char, palw, plane):
    """Return a 512x512 list-of-rows of (r,g,b,a); pixel 0 transparent."""
    base = plane * 0x1000
    px = bytearray(512 * 512 * 4)
    for ty in range(64):
        for tx in range(64):
            val = tile[base + ty * 64 + tx]
            charno = val & TILE_MASK
            colour = (val >> 7) & 0xff
            toff = charno * 32
            for row in range(8):
                ro = toff + row * 4
                if ro + 4 > len(char):
                    continue
                oy = (ty * 8 + row) * 512
                for col in range(8):
                    b = char[ro + (col >> 1)]
                    p = (b & 0x0f) if (col & 1) == 0 else (b >> 4)
                    if p == 0:
                        continue
                    r, g, bl = pal_rgb(palw, colour * 16 + p)
                    o = (oy + tx * 8 + col) * 4
                    px[o] = r; px[o + 1] = g; px[o + 2] = bl; px[o + 3] = 255
    return px


def composite(base, over):
    out = bytearray(base)
    for i in range(0, len(over), 4):
        if over[i + 3]:
            out[i:i + 4] = over[i:i + 4]
    return out


def scroll_of(tile, t):
    hs = tile[0x5000 + t]
    vs = tile[0x5004 + t]
    return ((-hs) & 0x1ff, vs & 0x1ff, bool(vs & 0x8000))


def main(argv):
    if len(argv) < 2:
        print(__doc__); return 2
    cap, out = argv[0], argv[1]
    bg_tm = int(_opt(argv, "--bg-tilemap", "1"))
    hud_tm = int(_opt(argv, "--hud-tilemap", "0"))
    wfront = "--wfront" in argv
    dump = _opt(argv, "--dump-layers", None)

    import PIL.Image as I
    tile, char, palw = load_capture(cap)

    planes = {p: decode_plane(tile, char, palw, p) for p in range(4)}
    if dump:
        os.makedirs(dump, exist_ok=True)
        for p in range(4):
            I.frombuffer("RGBA", (512, 512), bytes(planes[p]), "raw", "RGBA", 0, 1).save(
                os.path.join(dump, "plane%d.png" % p))

    s_plane, w_plane = bg_tm * 2, bg_tm * 2 + 1
    back, front = (s_plane, w_plane) if wfront else (w_plane, s_plane)
    bg = composite(planes[back], planes[front])
    img = I.frombuffer("RGBA", (512, 512), bytes(bg), "raw", "RGBA", 0, 1).convert("RGB")
    img.save(out)

    hx, hy, hdis = scroll_of(tile, bg_tm)
    hud_present = any(planes[hud_tm * 2 + k][3::4].count(255) for k in range(2))
    print("tilemap background -> %s (512x512)" % out)
    print("  background = tilemap %d (planes 0x%04x/0x%04x); HUD dropped = tilemap %d"
          % (bg_tm, s_plane * 0x1000, w_plane * 0x1000, hud_tm))
    print("  scroll (x,y) = (%d,%d)%s; on-screen window %dx%d at that scroll"
          % (hx, hy, " [DISABLED]" if hdis else "", SCREEN_W, SCREEN_H))
    print("  placement: a flat 2D backdrop (not a 3D sky). In Blender put the PNG on a large")
    print("  plane (or shallow cylinder) square behind the decor, centred on the camera and")
    print("  scaled so the window above the horizon fills the frame; it does not parallax.")
    if hud_present:
        print("  note: HUD tilemap %d has content (expected: life bars/timer/names) -- excluded."
              % hud_tm)
    return 0


def _opt(argv, name, default):
    if name in argv:
        i = argv.index(name)
        return argv[i + 1] if i + 1 < len(argv) else default
    return default


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
