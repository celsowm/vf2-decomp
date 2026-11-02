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
                                   [--dump-layers <dir>] [--obj] [--depth Z] [--blender]

The PNG is the full 512x512 backdrop panorama (sky + mountains above the horizon; the band
below the horizon is covered in-game by the 3D arena). It is a FLAT 2D layer, not a 3D sky,
so place it on a large flat plane (or a shallow cylinder) square behind the decor, filling
the camera frame; the on-screen window is 496x384 at the reported scroll. See the printed
note. Local tool; the PNGs it writes are snapshots and are never committed.

--obj also writes `bg_plane.obj` + `bg_plane.mtl` next to <out.png>: a single flat textured
quad (the backdrop PNG on it), in the SAME coordinate frame as the stage OBJ export
(feet-centred, +X -> screen-left, looking +Z -- the ext_emit_poly convention of a negated X
and reversed winding), so it imports into Blender already aligned behind the decor. The quad
sits at a fixed depth behind the decor (read from a `stage.obj` next to <out.png> if present,
else --depth, else a safe default) and is sized so the on-screen 496x384 tilemap window
exactly fills the game-camera frame; the full 512x512 texture extends just past the frame
edges. It is flat -- no parallax. Render it with a z-buffer (nearer = smaller Z), not painter
order, so the decor occludes it correctly.

--blender writes `blender_scene.py` next to <out.png>: a Blender script (run in the Scripting
tab, or `blender -b -P blender_scene.py -f 1`) that imports stage.obj + bg_plane.obj from the
same folder (forward -Z / up Y), builds the game camera VF2_GameCam (the placement camera above,
converted to Blender axes; lens from scale=430 over a 640-px reference, sensor 36mm), sets the
render to 496x384 with the Standard view transform, and makes every material unlit (Emission) so
the baked stage textures and the flat backdrop are shown as-is rather than washed out by scene
lighting. The OBJ has no camera, so this restores the game view Blender's free viewport cannot.
"""
import sys, os, struct

TILE_MASK = 0x3fff
SCREEN_W, SCREEN_H = 496, 384
TILE_W = TILE_H = 512

# Placement camera: the game-view approximation that reproduces screen.png from the stage
# OBJ export (feet-centred, camera removed). +X -> screen-left, +Y up, looking +Z; the raster
# it fills (RW x RH) corresponds to the full game screen. Tuned against stage decor + MAME.
CAM = dict(cx=0.0, cy=6.0, cz=-11.0, scale=430.0, rw=640, rh=480)
SENSOR_W = 36.0        # Blender sensor width (mm) used to turn `scale` into a focal length
DEFAULT_DEPTH = 400.0  # world Z of the backdrop quad when no stage.obj is found (decor max Z < ~200)


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


def decor_max_z(objpath):
    """Farthest decor Z in a stage OBJ (export frame), or None if unreadable."""
    try:
        mz = None
        for ln in open(objpath, encoding="utf-8", errors="replace"):
            if ln.startswith("v "):
                z = float(ln.split()[3])
                if mz is None or z > mz:
                    mz = z
        return mz
    except Exception:
        return None


def write_quad_obj(outpng, depth):
    """Write bg_plane.obj + bg_plane.mtl next to outpng: one flat textured quad at world Z=depth,
    sized so the 496x384 on-screen tilemap window exactly fills the placement-camera frame, the
    full 512x512 texture extending just past the edges. Same frame as the stage export
    (+X -> screen-left), so it imports behind the decor already aligned. Returns the obj path."""
    outdir = os.path.dirname(os.path.abspath(outpng))
    png = os.path.basename(outpng)
    stem = "bg_plane"
    cx, cy, cz, scale, rw, rh = (CAM[k] for k in ("cx", "cy", "cz", "scale", "rw", "rh"))
    dz = depth - cz                      # camera-space depth of the quad
    xh = dz * rw / (2.0 * scale)         # frustum half-width  at that depth
    yh = dz * rh / (2.0 * scale)         # frustum half-height at that depth
    # The on-screen window (tilemap cols 0..WIN_W, rows 0..WIN_H) fills the frame:
    #   col 0  -> screen-left  (x = cx + xh),  col WIN_W -> screen-right (x = cx - xh)
    #   row 0  -> screen-top   (y = cy + yh),  row WIN_H -> screen-bottom(y = cy - yh)
    # Extend linearly to the full 512x512 texture so UVs stay the plain 0..1 of bg.png.
    xcol = lambda c: (cx + xh) - (2.0 * xh) * (c / SCREEN_W)
    yrow = lambda r: (cy + yh) - (2.0 * yh) * (r / SCREEN_H)
    xl, xr = xcol(0), xcol(TILE_W)       # texture left / right edge in world X
    yt, yb = yrow(0), yrow(TILE_H)       # texture top / bottom edge in world Y
    z = depth
    obj = os.path.join(outdir, stem + ".obj")
    with open(obj, "w", encoding="utf-8") as f:
        f.write("# VF2 2D backdrop quad (flat, no parallax). Same frame as the stage OBJ export\n")
        f.write("# (feet-centred, +X -> screen-left, looking +Z). Place behind the decor; render\n")
        f.write("# with a z-buffer. World Z = %.3f. Local only, never committed.\n" % z)
        f.write("mtllib %s.mtl\n" % stem)
        f.write("o bg_plane\n")
        f.write("usemtl bg_plane\n")
        for (x, y) in ((xl, yt), (xr, yt), (xr, yb), (xl, yb)):   # TL, TR, BR, BL
            f.write("v %.6f %.6f %.6f\n" % (x, y, z))
        for (u, v) in ((0.0, 1.0), (1.0, 1.0), (1.0, 0.0), (0.0, 0.0)):
            f.write("vt %.6f %.6f\n" % (u, v))
        # camera-facing winding (normal toward -Z, the camera side): TL, BL, BR, TR
        f.write("f 1/1 4/4 3/3 2/2\n")
    with open(os.path.join(outdir, stem + ".mtl"), "w", encoding="utf-8") as f:
        f.write("# VF2 2D backdrop material. Local only, never committed.\n")
        f.write("newmtl bg_plane\nillum 1\nd 1.0\nKd 1.0 1.0 1.0\n")
        f.write("map_Kd %s\n" % png)
    return obj, z


BLENDER_TEMPLATE = r'''"""VF2 stage + 2D backdrop -> game camera, unlit, @@RESW@@x@@RESH@@. GENERATED by tilemap_bg.py.

Run it in Blender's Scripting tab (open this file from disk so __file__ is set, then press Run),
or headless from this folder:
    blender -b -P blender_scene.py -f 1
It imports stage.obj + bg_plane.obj from this same folder with forward -Z / up Y (the axes of
the manual import), builds the game camera VF2_GameCam, makes every material unlit (Emission)
so the baked stage textures and the flat backdrop show as-is (no washing-out), sets the render
to @@RESW@@x@@RESH@@ with the Standard view transform, and renders from VF2_GameCam.
Set VF2_ENGINE=CYCLES in the environment to force Cycles (reliable headless). Local, never committed.
"""
import bpy, os, math, mathutils

try:
    HERE = os.path.dirname(os.path.abspath(__file__))
except NameError:
    HERE = os.path.dirname(bpy.data.filepath) or bpy.path.abspath("//")

STAGE_OBJ = os.path.join(HERE, "@@STAGEOBJ@@")
BG_OBJ    = os.path.join(HERE, "@@BGOBJ@@")

for ob in list(bpy.data.objects):
    bpy.data.objects.remove(ob, do_unlink=True)

def import_obj(path):
    try:
        bpy.ops.wm.obj_import(filepath=path, forward_axis='NEGATIVE_Z', up_axis='Y')
    except Exception:
        bpy.ops.import_scene.obj(filepath=path, axis_forward='-Z', axis_up='Y')

import_obj(STAGE_OBJ)
import_obj(BG_OBJ)

# Placement camera (reproduces screen.png from the stage export). In the OBJ/export frame it is
# at (@@OCAMX@@,@@OCAMY@@,@@OCAMZ@@) looking +Z, up +Y, focal scale=@@SCALE@@ over a @@REFW@@-px-wide
# reference raster. The forward -Z / up Y import maps OBJ (x,y,z) -> Blender (x, -z, y), so in
# Blender the camera sits at (@@CAMX@@,@@CAMY@@,@@CAMZ@@) looking -Y with up +Z (OBJ +X stays
# screen-left). Reference width @@REFW@@ px, sensor width @@SENSORW@@ mm ->
#   lens = (sensor/2) / ((refw/2)/scale) = @@LENS@@ mm.
cam_data = bpy.data.cameras.new("VF2_GameCam")
cam_data.sensor_fit = 'HORIZONTAL'
cam_data.sensor_width = @@SENSORW@@
cam_data.lens = @@LENS@@
cam = bpy.data.objects.new("VF2_GameCam", cam_data)
bpy.context.scene.collection.objects.link(cam)
cam.location = (@@CAMX@@, @@CAMY@@, @@CAMZ@@)
# camera local axes in world: right -> -X, up -> +Z, view(-Z) -> -Y  (look -Y, up +Z)
cam.rotation_euler = mathutils.Matrix(((-1.0, 0.0, 0.0),
                                       (0.0, 0.0, 1.0),
                                       (0.0, 1.0, 0.0))).to_euler()
bpy.context.scene.camera = cam

scn = bpy.context.scene
scn.render.resolution_x = @@RESW@@
scn.render.resolution_y = @@RESH@@
scn.render.resolution_percentage = 100
scn.render.image_settings.file_format = 'PNG'
scn.render.filepath = os.path.join(HERE, "render_gamecam_")
engines = [os.environ['VF2_ENGINE']] if os.environ.get('VF2_ENGINE') else \
          ['BLENDER_EEVEE_NEXT', 'BLENDER_EEVEE', 'CYCLES']
for eng in engines:
    try:
        scn.render.engine = eng
        break
    except Exception:
        pass
scn.view_settings.view_transform = 'Standard'
if bpy.data.worlds:
    scn.world = bpy.data.worlds[0]
    try:
        scn.world.use_nodes = True
        scn.world.node_tree.nodes["Background"].inputs[0].default_value = (0, 0, 0, 1)
    except Exception:
        pass

def make_unlit(mat):
    if not mat:
        return
    mat.use_nodes = True
    nt = mat.node_tree
    tex = next((n for n in nt.nodes if n.type == 'TEX_IMAGE'), None)
    if tex is None:
        return
    for n in list(nt.nodes):
        if n.type in ('BSDF_PRINCIPLED', 'OUTPUT_MATERIAL', 'EMISSION', 'MIX_SHADER',
                      'BSDF_TRANSPARENT'):
            nt.nodes.remove(n)
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    emi = nt.nodes.new('ShaderNodeEmission')
    emi.inputs['Strength'].default_value = 1.0
    nt.links.new(tex.outputs['Color'], emi.inputs['Color'])
    trans = nt.nodes.new('ShaderNodeBsdfTransparent')
    mix = nt.nodes.new('ShaderNodeMixShader')
    nt.links.new(tex.outputs['Alpha'], mix.inputs['Fac'])   # 1.0 when the image has no alpha
    nt.links.new(trans.outputs[0], mix.inputs[1])
    nt.links.new(emi.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs['Surface'])
    for attr, val in (('blend_method', 'BLEND'), ('surface_render_method', 'BLEND'),
                      ('use_backface_culling', False)):
        try:
            setattr(mat, attr, val)
        except Exception:
            pass

for mat in bpy.data.materials:
    make_unlit(mat)

print("VF2 scene ready: VF2_GameCam, %dx%d, %s, Standard, unlit. Render -> %s0001.png"
      % (scn.render.resolution_x, scn.render.resolution_y, scn.render.engine, scn.render.filepath))
'''


def write_blender_scene(outpng, stage_obj_name="stage.obj", bg_obj_name="bg_plane.obj"):
    """Write blender_scene.py next to outpng: import stage.obj + bg_plane.obj, build the game
    camera VF2_GameCam, make materials unlit, 496x384 + Standard. Returns its path."""
    cx, cy, cz, scale, rw, rh = (CAM[k] for k in ("cx", "cy", "cz", "scale", "rw", "rh"))
    lens = (SENSOR_W / 2.0) / ((rw / 2.0) / scale)
    repl = {
        "@@STAGEOBJ@@": stage_obj_name, "@@BGOBJ@@": bg_obj_name,
        "@@OCAMX@@": "%g" % cx, "@@OCAMY@@": "%g" % cy, "@@OCAMZ@@": "%g" % cz,
        "@@SCALE@@": "%g" % scale, "@@REFW@@": "%d" % rw, "@@SENSORW@@": "%g" % SENSOR_W,
        "@@LENS@@": "%.4f" % lens,
        "@@CAMX@@": "%g" % cx, "@@CAMY@@": "%g" % (-cz), "@@CAMZ@@": "%g" % cy,
        "@@RESW@@": "%d" % SCREEN_W, "@@RESH@@": "%d" % SCREEN_H,
    }
    s = BLENDER_TEMPLATE
    for k, v in repl.items():
        s = s.replace(k, v)
    path = os.path.join(os.path.dirname(os.path.abspath(outpng)), "blender_scene.py")
    with open(path, "w", encoding="utf-8") as f:
        f.write(s)
    return path, lens


def main(argv):
    if len(argv) < 2:
        print(__doc__); return 2
    cap, out = argv[0], argv[1]
    bg_tm = int(_opt(argv, "--bg-tilemap", "1"))
    hud_tm = int(_opt(argv, "--hud-tilemap", "0"))
    wfront = "--wfront" in argv
    dump = _opt(argv, "--dump-layers", None)
    want_obj = "--obj" in argv
    want_blender = "--blender" in argv
    depth_opt = _opt(argv, "--depth", None)

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

    if want_obj:
        if depth_opt is not None:
            depth = float(depth_opt)
            src = "--depth"
        else:
            mz = decor_max_z(os.path.join(os.path.dirname(os.path.abspath(out)), "stage.obj"))
            if mz is not None:
                depth = mz * 1.3 + 30.0   # comfortably behind the farthest decor vertex
                src = "stage.obj max Z=%.1f" % mz
            else:
                depth = DEFAULT_DEPTH
                src = "default (no stage.obj)"
        objp, z = write_quad_obj(out, depth)
        print("  quad: %s + bg_plane.mtl -> flat backdrop at world Z=%.1f (%s)" % (objp, z, src))
        print("        camera-facing, +X=screen-left; the 496x384 window fills the game frame.")

    if want_blender:
        sp, lens = write_blender_scene(out)
        print("  blender: %s -> imports stage.obj + bg_plane.obj, VF2_GameCam (lens %.2fmm," % (sp, lens))
        print("           %dx%d, Standard, unlit). Run: blender -b -P blender_scene.py -f 1" % (SCREEN_W, SCREEN_H))
    return 0


def _opt(argv, name, default):
    if name in argv:
        i = argv.index(name)
        return argv[i + 1] if i + 1 < len(argv) else default
    return default


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
