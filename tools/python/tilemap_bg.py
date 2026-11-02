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

--obj and --blender use the EXACT game camera of the capture, read from `camera.txt` beside
<out.png> (geo-stage emits it next to stage.obj): view = fc.A + T with fc=(-OBJ.x,OBJ.y,OBJ.z),
then the Model 2 projection (fx,fy,px,py over WxH). No camera is eyeballed. Run geo-stage on the
same capture first so camera.txt and stage.obj are present and consistent with screen.png.

--obj writes `bg_plane.obj` + `bg_plane.mtl` next to <out.png>: a single flat textured quad
carrying the backdrop PNG, built in the camera's VIEW space (a plane at a depth behind the decor,
sized so the 496x384 on-screen tilemap window exactly fills the game frame, the full 512x512
texture extending just past the edges) and mapped to OBJ coords, so it imports into Blender
aligned with stage.obj. Flat -- no parallax. Render z-buffered so the decor occludes it and the
backdrop shows only through the decor's openings. --depth Z overrides the view depth.

--blender writes `blender_scene.py` next to <out.png>: a Blender script (run in the Scripting
tab, or `blender -b -P blender_scene.py -f 1`) that imports stage.obj + bg_plane.obj from the
same folder (forward -Z / up Y), builds the exact game camera VF2_GameCam (camera.txt converted
to a Blender pose + lens/shift), sets the render to WxH with the Standard view transform, and
makes every material unlit (Emission) so the baked stage textures and the flat backdrop are shown
as-is rather than washed out by scene lighting. The OBJ carries no camera, so this restores the
game view Blender's free viewport cannot.
"""
import sys, os, struct, math

TILE_MASK = 0x3fff
SCREEN_W, SCREEN_H = 496, 384
TILE_W = TILE_H = 512

# Placement camera: the game-view approximation that reproduces screen.png from the stage
# OBJ export (feet-centred, camera removed). +X -> screen-left, +Y up, looking +Z; the raster
# it fills (RW x RH) corresponds to the full game screen. Tuned against stage decor + MAME.
SENSOR_W = 36.0        # Blender sensor width (mm) used to turn the pixel focal into a lens length
MERGE_DIST = 0.15      # Merge-by-Distance weld threshold (world units) for the stage decor in
                       # blender_scene.py. Measured on stage 4: crack pairs sit at <= 0.106 u and
                       # the smallest real feature at 0.37 u, a clean gap -- 0.15 closes every
                       # crack and touches no feature. Decals are separate quads (no near-dup with
                       # the wall), so they are not merged. ~0.6 px at the wall.


def read_camera(path):
    """Parse camera.txt (emitted by geo-stage): the exact game view transform of the capture.
    view = fc . A + T with fc = (-OBJ.x, OBJ.y, OBJ.z); projection sx = px + fx*vx/vz,
    sy = py - fy*vy/vz over W x H. Returns a dict, or None if the file is missing."""
    try:
        A = T = None
        fx = fy = 600.0
        px = 248.0
        py = 195.0
        W = SCREEN_W
        H = SCREEN_H
        for ln in open(path, encoding="utf-8"):
            s = ln.split()
            if not s or s[0] == "#":
                continue
            if s[0] == "A":
                A = [float(x) for x in s[1:10]]
            elif s[0] == "T":
                T = [float(x) for x in s[1:4]]
            elif s[0] == "proj":
                kv = {s[i]: s[i + 1] for i in range(1, len(s) - 1, 2)}
                fx = float(kv.get("fx", fx)); fy = float(kv.get("fy", fy))
                px = float(kv.get("px", px)); py = float(kv.get("py", py))
                W = int(kv.get("W", W)); H = int(kv.get("H", H))
        if A is None or T is None:
            return None
        return dict(A=A, T=T, fx=fx, fy=fy, px=px, py=py, W=W, H=H)
    except Exception:
        return None


def mat3_inv(a):
    """Inverse of a row-major 3x3 (a[3i+j]); returns a 9-list, or None if singular."""
    det = (a[0] * (a[4] * a[8] - a[5] * a[7])
           - a[1] * (a[3] * a[8] - a[5] * a[6])
           + a[2] * (a[3] * a[7] - a[4] * a[6]))
    if abs(det) < 1e-20:
        return None
    d = 1.0 / det
    return [
        (a[4] * a[8] - a[5] * a[7]) * d, (a[2] * a[7] - a[1] * a[8]) * d, (a[1] * a[5] - a[2] * a[4]) * d,
        (a[5] * a[6] - a[3] * a[8]) * d, (a[0] * a[8] - a[2] * a[6]) * d, (a[2] * a[3] - a[0] * a[5]) * d,
        (a[3] * a[7] - a[4] * a[6]) * d, (a[1] * a[6] - a[0] * a[7]) * d, (a[0] * a[4] - a[1] * a[3]) * d,
    ]


def obj_to_view(cam, ox, oy, oz):
    """OBJ vertex -> Model 2 camera-space (vx,vy,vz). fc = (-x,y,z); view = fc . A + T."""
    A, T = cam["A"], cam["T"]
    fx_, fy_, fz_ = -ox, oy, oz
    vx = fx_ * A[0] + fy_ * A[3] + fz_ * A[6] + T[0]
    vy = fx_ * A[1] + fy_ * A[4] + fz_ * A[7] + T[1]
    vz = fx_ * A[2] + fy_ * A[5] + fz_ * A[8] + T[2]
    return vx, vy, vz


def view_to_obj(cam, vx, vy, vz, ainv):
    """Model 2 camera-space -> OBJ vertex. fc = (view - T) . A^-1 ; OBJ = (-fc.x, fc.y, fc.z)."""
    T = cam["T"]
    dx, dy, dz = vx - T[0], vy - T[1], vz - T[2]
    fcx = dx * ainv[0] + dy * ainv[3] + dz * ainv[6]
    fcy = dx * ainv[1] + dy * ainv[4] + dz * ainv[7]
    fcz = dx * ainv[2] + dy * ainv[5] + dz * ainv[8]
    return (-fcx, fcy, fcz)


def decor_max_vz(objpath, cam):
    """Largest camera-space depth vz over a stage OBJ's vertices (for placing the backdrop
    behind the decor). Returns None if unreadable."""
    try:
        mz = None
        for ln in open(objpath, encoding="utf-8", errors="replace"):
            if ln.startswith("v "):
                _, x, y, z = ln.split()
                vz = obj_to_view(cam, float(x), float(y), float(z))[2]
                if vz > 0.01 and (mz is None or vz > mz):
                    mz = vz
        return mz
    except Exception:
        return None


def _u16(path, nwords):
    return struct.unpack("<%dH" % nwords, open(path, "rb").read())


# colorxlat (model2.cpp palette handler): a captured 5-5-5 palette entry is NOT expanded
# bit-for-bit; each 5-bit channel is looked up in the colorxlat LUT, word index base + (v<<8),
# base = 0x40 (R), 0x2040 (G), 0x4040 (B); the entry's low byte is the 8-bit value.
CX_BASE = (0x40, 0x2040, 0x4040)


def _screen_gamma(v):
    """Fixed VF2 screen DAC gamma (MEASURED, same as the 3D pipeline): max((v-64)*255/191, 0)."""
    x = int((v - 64) * 255.0 / 191.0 + 0.5)
    return 0 if x < 0 else (255 if x > 255 else x)


# gamma(colorxlat(0..255)) precomputed per channel once a capture's colorxlat is known
def load_capture(cap):
    tile = _u16(os.path.join(cap, "tileram_01000000.bin"), 0x10000 // 2)
    char = open(os.path.join(cap, "charram_01080000.bin"), "rb").read()
    palw = _u16(os.path.join(cap, "palette_01800000.bin"), 0x4000 // 2)
    cxpath = os.path.join(cap, "colorxlat_01810000.bin")
    cx = _u16(cxpath, 0xc000 // 2) if os.path.isfile(cxpath) else None
    return tile, char, palw, cx


def pal_rgb(palw, cx, idx):
    """Palette entry -> on-screen RGB: 5-5-5 -> colorxlat LUT -> screen gamma (the hardware path).
    Without a colorxlat capture, falls back to the raw 5-bit<<3 expansion (too bright)."""
    v = palw[idx & 0xfff] & 0x7fff
    ch = (v & 31, (v >> 5) & 31, (v >> 10) & 31)
    if cx is None:
        return (ch[0] << 3, ch[1] << 3, ch[2] << 3)
    return tuple(_screen_gamma(cx[CX_BASE[i] + (ch[i] << 8)] & 0xff) for i in range(3))


def decode_plane(tile, char, palw, cx, plane):
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
                    r, g, bl = pal_rgb(palw, cx, colour * 16 + p)
                    o = (oy + tx * 8 + col) * 4
                    px[o] = r; px[o + 1] = g; px[o + 2] = bl; px[o + 3] = 255
    return px


def composite(base, over):
    out = bytearray(base)
    for i in range(0, len(over), 4):
        if over[i + 3]:
            out[i:i + 4] = over[i:i + 4]
    return out


def plane_scroll(tile, plane):
    """segas24 per-PLANE scroll: hscroll word 0x5000+plane, vscroll word 0x5004+plane; the scroll
    is bits 0-8, hscroll bit 15 = per-row scroll enabled. Screen pixel (sx,sy) shows tilemap pixel
    (sx+hscroll, sy+vscroll), i.e. tilemap (col,row) appears at screen (col-hscroll, row-vscroll)."""
    hs = tile[0x5000 + plane]
    vs = tile[0x5004 + plane]
    return (hs & 0x1ff, vs & 0x1ff, bool(hs & 0x8000))


def write_quad_obj(outpng, cam, hscroll=0, vscroll=0, depth=None):
    """Write bg_plane.obj + bg_plane.mtl next to outpng: one flat textured quad on the real game
    camera's view axis, behind the decor, sized so the on-screen 496x384 tilemap window exactly
    fills the game frame (the full 512x512 texture extends just past the edges). Built in VIEW
    space from `cam`, then mapped to OBJ coords (fc=(view-T).A^-1, OBJ=(-fc.x,fc.y,fc.z)), so it
    imports into Blender aligned with stage.obj and appears only through the decor's openings.
    Returns (obj_path, view_depth)."""
    outdir = os.path.dirname(os.path.abspath(outpng))
    png = os.path.basename(outpng)
    stem = "bg_plane"
    ainv = mat3_inv(cam["A"])
    if ainv is None:
        raise ValueError("camera matrix A is singular")
    if depth is None:
        mz = decor_max_vz(os.path.join(outdir, "stage.obj"), cam)
        depth = (mz * 1.10 + 5.0) if mz is not None else 400.0
    fx, fy, px, py = cam["fx"], cam["fy"], cam["px"], cam["py"]
    z = depth
    # Texel (col,row) of the 512x512 tilemap is shown on screen at (col-hscroll, row-vscroll)
    # (segas24 scroll); the 496x384 window is tilemap cols/rows [hscroll,+496)/[vscroll,+384). Map
    # each of the four texture corners to its screen position, then to a view point at depth z:
    #   vx = (sx - px) * z / fx ,  vy = (py - sy) * z / fy ,  vz = z.
    def corner(col, row):
        sx = col - hscroll
        sy = row - vscroll
        vx = (sx - px) * z / fx
        vy = (py - sy) * z / fy
        return view_to_obj(cam, vx, vy, z, ainv)
    tl = corner(0.0, 0.0); tr = corner(TILE_W, 0.0)
    br = corner(TILE_W, TILE_H); bl = corner(0.0, TILE_H)
    obj = os.path.join(outdir, stem + ".obj")
    with open(obj, "w", encoding="utf-8") as f:
        f.write("# VF2 2D backdrop quad (flat, no parallax), on the game camera's view axis at\n")
        f.write("# view depth %.3f, behind the decor. Same frame as stage.obj; render z-buffered\n" % z)
        f.write("# so the decor occludes it (the backdrop shows only through openings).\n")
        f.write("# Local only, never committed.\n")
        f.write("mtllib %s.mtl\n" % stem)
        f.write("o bg_plane\n")
        f.write("usemtl bg_plane\n")
        for v in (tl, tr, br, bl):
            f.write("v %.6f %.6f %.6f\n" % (v[0], v[1], v[2]))
        for (u, vv) in ((0.0, 1.0), (1.0, 1.0), (1.0, 0.0), (0.0, 0.0)):   # TL, TR, BR, BL
            f.write("vt %.6f %.6f\n" % (u, vv))
        f.write("f 1/1 2/2 3/3 4/4\n")
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

_GEN_DIR = r"@@GENDIR@@"   # absolute folder this script was generated into (final fallback)
_STAGE = "@@STAGEOBJ@@"

def _has_stage(d):
    return bool(d) and os.path.isfile(os.path.join(d, _STAGE))

def _here():
    """Folder holding stage.obj: the first candidate that actually contains it -- __file__'s dir
    (blender -P), the open text's dir (Text Editor Run), the .blend dir, then cwd. Each candidate
    is checked for stage.obj, because in the Scripting tab __file__ can be "\\blender_scene.py"
    (dir "<drive>:\\"), which has none, and the real folder must still be found."""
    cands = []
    try:
        cands.append(os.path.dirname(os.path.abspath(__file__)))
    except NameError:
        pass
    try:
        txt = bpy.context.space_data.text
        if txt and txt.filepath:
            cands.append(os.path.dirname(bpy.path.abspath(txt.filepath)))
    except Exception:
        pass
    if bpy.data.filepath:
        cands.append(os.path.dirname(bpy.data.filepath))
    cands.append(os.getcwd())
    for c in cands:
        if _has_stage(c):
            return c
    return cands[0] if cands else _GEN_DIR

HERE = _here()
if not _has_stage(HERE):
    HERE = _GEN_DIR   # none of the live candidates held stage.obj -> use the baked-in folder

STAGE_OBJ = os.path.join(HERE, "@@STAGEOBJ@@")
BG_OBJ    = os.path.join(HERE, "@@BGOBJ@@")

for ob in list(bpy.data.objects):
    bpy.data.objects.remove(ob, do_unlink=True)

def import_obj(path):
    bpy.ops.wm.obj_import(filepath=path, forward_axis='NEGATIVE_Z', up_axis='Y')

import_obj(STAGE_OBJ)
import_obj(BG_OBJ)

# Weld the decor. The export writes each polygon's vertices independently, and adjacent wall
# polygons -- within a record and across records -- can carry the SAME corner at coordinates that
# differ by fixed-point rounding (measured: gaps 0.0008..0.1 u, ~0.003..0.4 px at the wall). Those
# hairline gaps let the backdrop (sky) show as thin lavender lines between polygons; MAME rasterises
# at whole pixels and never shows them. Join the stage pieces (NOT bg_plane) and Merge by Distance
# at @@MERGEDIST@@ u (sub-pixel at the wall, far below any real feature) to close them. Per-corner
# UVs are loop data, so welding positions does not move textures.
def weld_stage(dist):
    import bmesh
    stage = [o for o in bpy.data.objects if o.type == 'MESH' and o.name != 'bg_plane'
             and not o.name.startswith('bg_plane')]
    if not stage:
        return 0
    bpy.ops.object.select_all(action='DESELECT')
    for o in stage:
        o.select_set(True)
    bpy.context.view_layer.objects.active = stage[0]
    if len(stage) > 1:
        bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    ob.name = "VF2_Stage"
    me = ob.data
    bm = bmesh.new(); bm.from_mesh(me)
    before = len(bm.verts)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=dist)
    welded = before - len(bm.verts)
    bm.to_mesh(me); bm.free(); me.update()
    return welded

_welded = weld_stage(@@MERGEDIST@@)
print("stage welded (merge by distance %g u): %d vertices removed" % (@@MERGEDIST@@, _welded))

# VF2_GameCam: the EXACT game camera of this capture, from camera.txt (geo-stage). The Model 2
# view transform (view = fc.A + T, fc=(-OBJ.x,OBJ.y,OBJ.z)) + projection (fx,fy,px,py over WxH)
# was converted to a Blender camera pose + lens. matrix_world and lens/shift are computed by
# tilemap_bg.py and baked in below, so no camera is eyeballed. OBJ (x,y,z) -> Blender (x,-z,y).
cam_data = bpy.data.cameras.new("VF2_GameCam")
cam_data.sensor_fit = 'HORIZONTAL'
cam_data.sensor_width = @@SENSORW@@
cam_data.lens = @@LENS@@
cam_data.shift_x = @@SHIFTX@@
cam_data.shift_y = @@SHIFTY@@
cam = bpy.data.objects.new("VF2_GameCam", cam_data)
bpy.context.scene.collection.objects.link(cam)
cam.matrix_world = mathutils.Matrix((
    (@@R00@@, @@R01@@, @@R02@@, @@CBX@@),
    (@@R10@@, @@R11@@, @@R12@@, @@CBY@@),
    (@@R20@@, @@R21@@, @@R22@@, @@CBZ@@),
    (0.0, 0.0, 0.0, 1.0)))
bpy.context.scene.camera = cam
bpy.context.scene.render.pixel_aspect_x = 1.0
bpy.context.scene.render.pixel_aspect_y = 1.0

scn = bpy.context.scene
scn.render.resolution_x = @@RESW@@
scn.render.resolution_y = @@RESH@@
scn.render.resolution_percentage = 100
scn.render.image_settings.file_format = 'PNG'
scn.render.filepath = os.path.join(HERE, "render_gamecam_")
# Cycles first: it ray-traces, so the coplanar wall decals (the banner) and the backdrop seen
# through the opening resolve by true depth. EEVEE's rasterizer mis-orders the coplanar decals
# (the banner comes out partial), so it is only a fast fallback. VF2_ENGINE overrides.
engines = [os.environ['VF2_ENGINE']] if os.environ.get('VF2_ENGINE') else \
          ['CYCLES', 'BLENDER_EEVEE_NEXT', 'BLENDER_EEVEE']
for eng in engines:
    try:
        scn.render.engine = eng
        break
    except Exception:
        pass
if scn.render.engine == 'CYCLES':
    try:
        scn.cycles.samples = 24          # emission-only: nearly noise-free, keep it fast
        scn.cycles.use_denoising = False
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
    tex.interpolation = 'Closest'   # MAME shows crisp pixels, not bilinear-blurred texels
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
    # The backdrop (bg_plane) is opaque and MUST write depth (DITHERED) so the decor z-occludes
    # it and it shows only through openings. The decor + its coplanar decals stay BLEND: they
    # draw after the opaque pass, over the backdrop where geometry exists, in painter order (like
    # MAME) -- so the banner decal shows on the wall instead of losing a z-fight to it.
    is_bg = (mat.name == 'bg_plane')
    settings = (('surface_render_method', 'DITHERED'), ('blend_method', 'CLIP')) if is_bg else \
               (('surface_render_method', 'BLEND'), ('blend_method', 'BLEND'))
    for attr, val in settings + (('use_backface_culling', False),):
        try:
            setattr(mat, attr, val)
        except Exception:
            pass

for mat in bpy.data.materials:
    make_unlit(mat)

print("VF2 scene ready: VF2_GameCam, %dx%d, %s, Standard, unlit. Render -> %s0001.png"
      % (scn.render.resolution_x, scn.render.resolution_y, scn.render.engine, scn.render.filepath))
'''


def blender_camera(cam):
    """Convert the Model 2 game camera (cam from camera.txt) to a Blender camera, expressed in
    the forward -Z / up Y import frame OBJ (x,y,z) -> Blender (x,-z,y). Returns
    (R 3x3 row-major, C 3-vector, lens_mm, shift_x, shift_y)."""
    A, T = cam["A"], cam["T"]
    fx, px, py, W, H = cam["fx"], cam["px"], cam["py"], cam["W"], cam["H"]
    ainv = mat3_inv(A)
    if ainv is None:
        raise ValueError("camera matrix A is singular")
    # view = L . P_blender (linear part). view_j = fc.A[:,j], fc = (-bx, bz, -by), so
    # dview_j/d(bx,by,bz) = (-A[0,j], -A[2,j], A[1,j]).  A[i,j] = A[3i+j].
    L = [-A[0], -A[6], A[3],
         -A[1], -A[7], A[4],
         -A[2], -A[8], A[5]]
    linv = mat3_inv(L)
    if linv is None:
        raise ValueError("camera linear map is singular")

    def norm(v):
        n = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) or 1.0
        return [v[0] / n, v[1] / n, v[2] / n]
    # Blender-world directions of the view axes = columns of L^-1.
    vxdir = norm([linv[0], linv[3], linv[6]])   # screen right
    vydir = norm([linv[1], linv[4], linv[7]])   # screen up
    vzdir = norm([linv[2], linv[5], linv[8]])   # depth (into screen)
    # Blender camera looks down local -Z, right +X, up +Y: local Z = -depth.
    R = [[vxdir[0], vydir[0], -vzdir[0]],
         [vxdir[1], vydir[1], -vzdir[1]],
         [vxdir[2], vydir[2], -vzdir[2]]]
    # optical centre: view = 0 at fc0 = -T.A^-1 (row-vector); map fc->Blender (fc=(-bx,bz,-by)).
    fc0 = [-(T[0] * ainv[0] + T[1] * ainv[3] + T[2] * ainv[6]),
           -(T[0] * ainv[1] + T[1] * ainv[4] + T[2] * ainv[7]),
           -(T[0] * ainv[2] + T[1] * ainv[5] + T[2] * ainv[8])]
    C = [-fc0[0], -fc0[2], fc0[1]]
    lens = fx * SENSOR_W / float(W)
    # Blender shift is a fraction of the larger sensor dimension (width, landscape). Image x
    # grows right like Blender's; image y grows down, Blender's up -- but the measured match
    # (banner aligns to the exact software reprojection) gives the same sign for both:
    shift_x = (px - W / 2.0) / float(W)
    shift_y = (py - H / 2.0) / float(W)
    return R, C, lens, shift_x, shift_y


def write_blender_scene(outpng, cam, stage_obj_name="stage.obj", bg_obj_name="bg_plane.obj"):
    """Write blender_scene.py next to outpng: import stage.obj + bg_plane.obj, build the exact
    game camera VF2_GameCam from `cam`, make materials unlit, WxH + Standard. Returns (path, lens)."""
    R, C, lens, sx, sy = blender_camera(cam)
    gen_dir = os.path.dirname(os.path.abspath(outpng)).replace("\\", "/")
    repl = {
        "@@GENDIR@@": gen_dir, "@@MERGEDIST@@": "%g" % MERGE_DIST,
        "@@STAGEOBJ@@": stage_obj_name, "@@BGOBJ@@": bg_obj_name,
        "@@SENSORW@@": "%g" % SENSOR_W, "@@LENS@@": "%.6f" % lens,
        "@@SHIFTX@@": "%.8f" % sx, "@@SHIFTY@@": "%.8f" % sy,
        "@@R00@@": "%.8f" % R[0][0], "@@R01@@": "%.8f" % R[0][1], "@@R02@@": "%.8f" % R[0][2],
        "@@R10@@": "%.8f" % R[1][0], "@@R11@@": "%.8f" % R[1][1], "@@R12@@": "%.8f" % R[1][2],
        "@@R20@@": "%.8f" % R[2][0], "@@R21@@": "%.8f" % R[2][1], "@@R22@@": "%.8f" % R[2][2],
        "@@CBX@@": "%.6f" % C[0], "@@CBY@@": "%.6f" % C[1], "@@CBZ@@": "%.6f" % C[2],
        "@@RESW@@": "%d" % cam["W"], "@@RESH@@": "%d" % cam["H"],
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
    tile, char, palw, cx = load_capture(cap)
    if cx is None:
        print("  WARNING: no colorxlat_01810000.bin -- colours use the raw 5-bit expansion (too"
              " bright); the real path is colorxlat + screen gamma.")

    planes = {p: decode_plane(tile, char, palw, cx, p) for p in range(4)}
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

    hx, hy, rowscroll = plane_scroll(tile, s_plane)   # scroll of the visible backdrop plane
    hud_present = any(planes[hud_tm * 2 + k][3::4].count(255) for k in range(2))
    print("tilemap background -> %s (512x512)" % out)
    print("  background = tilemap %d (planes 0x%04x/0x%04x); HUD dropped = tilemap %d"
          % (bg_tm, s_plane * 0x1000, w_plane * 0x1000, hud_tm))
    print("  scroll (x,y) = (%d,%d)%s; on-screen window = tilemap [%d..%d) x [%d..%d)"
          % (hx, hy, " [ROW-SCROLL]" if rowscroll else "", hx, hx + SCREEN_W, hy, hy + SCREEN_H))
    if rowscroll:
        print("  note: per-row scroll is enabled on this plane; the flat quad uses the base scroll")
        print("        only (any per-line wobble is not reproduced).")
    if hud_present:
        print("  note: HUD tilemap %d has content (expected: life bars/timer/names) -- excluded."
              % hud_tm)

    cam = None
    if want_obj or want_blender:
        campath = os.path.join(os.path.dirname(os.path.abspath(out)), "camera.txt")
        cam = read_camera(campath)
        if cam is None:
            print("  ERROR: --obj/--blender need the game camera. Run geo-stage on this capture to\n"
                  "         produce %s (it emits camera.txt beside stage.obj)." % campath)
            return 3

    if want_obj:
        depth = float(depth_opt) if depth_opt is not None else None
        objp, z = write_quad_obj(out, cam, hx, hy, depth)
        src = "--depth" if depth_opt is not None else "stage.obj decor depth"
        print("  quad: %s + bg_plane.mtl -> flat backdrop at view depth %.1f (%s)" % (objp, z, src))
        print("        on the game camera's view axis; shows only through the decor's openings (z-buffer).")

    if want_blender:
        sp, lens = write_blender_scene(out, cam)
        print("  blender: %s -> imports stage.obj + bg_plane.obj, exact VF2_GameCam (lens %.2fmm," % (sp, lens))
        print("           %dx%d, Standard, unlit). Run: blender -b -P blender_scene.py -f 1" % (cam["W"], cam["H"]))
    return 0


def _opt(argv, name, default):
    if name in argv:
        i = argv.index(name)
        return argv[i + 1] if i + 1 < len(argv) else default
    return default


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
