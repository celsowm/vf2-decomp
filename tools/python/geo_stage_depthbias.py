"""Coplanar depth-bias for a stage OBJ (painter-order z-fight fix).

VF2's Model 2 renderer draws decor in display-list order with exact arithmetic, so
coplanar faces never z-fight on the arcade. A baked OBJ viewed with a float z-buffer
(Blender) does: two coplanar faces shimmer in a thin zigzag along their shared area.
This happens between captured objects of one frame (seen already in the single-frame
stage_NN export) and, in the from-ROM stage_NN_full, between recovered and captured
decor that lie on the same plane.

Fix (kazuyette's rule): where faces are coplanar, the face drawn FIRST in the display
list is pushed BACK (behind the later one), mirroring MAME's painter order; and a
recovered object (_rom_) always goes behind a captured one (_cap_). The front-most face
in each coplanar group stays put; the others are nudged a hair along -normal so the
intended face wins the depth test. The geometriser emits every face with its own
vertices (no sharing), so a per-face nudge never tears the mesh.

Two kinds of coplanar conflict are resolved:
  - BETWEEN distinct objects whose in-plane boxes touch or overlap (a seam, or a decor
    object lying on another's plane) -- the object drawn later, or the captured one over
    a recovered one, stays in front;
  - WITHIN a single object, a face that AREA-overlaps an earlier-drawn face of the same
    object is a decal painted onto that base (a sign on its wall, a plaque on a beam) --
    the decal is lifted a hair TOWARD THE ORIGIN (the arena interior, hence toward the
    viewer) so it wins the depth test. This is what keeps the stage-4 Great Wall banner
    from z-fighting the brick header it is painted onto. Abutting tiles of one object
    share only an edge (no area), so a tiled floor is left exactly as drawn.

Fighters are never run through this (stage tool only), so character OBJs stay
byte-identical.

Usage: python geo_stage_depthbias.py <stage.obj>   (rewrites in place)
       or import and call depth_bias(obj_path).
"""
import sys
import math

EPS = 0.06          # world-units nudge per painter layer (invisible, > float-z epsilon)
NQ = 0.05           # normal-direction quantum for the plane bucket key
DQ = 0.75           # plane-offset quantum for the coarse coplanar bucket
TIE = 0.0015        # faces within this depth are a z-fighting tie; beyond it one is in
                    # front (a lifted translucent decal, deps >= 0.003, is never a tie
                    # with its base, so it is never pushed behind -- "recule l'opaque").
GAP = 0.5           # two coplanar faces conflict when their in-plane boxes are within this
                    # gap (touching/abutting -> a seam z-fight, or overlapping). Faces far
                    # apart on the same plane (the central slab vs wall-base trim) are left
                    # alone, so the slab keeps its height instead of being sunk.
OVM = 0.05          # intra-object: a later face must overlap the base by this much in-plane
                    # AREA to count as a decal on it. Abutting tiles (shared edge, zero
                    # area) stay below it and are never nudged.


def _face_normal(vs):
    a, b, c = vs[0], vs[1], vs[2]
    e1 = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
    e2 = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
    n = (e1[1] * e2[2] - e1[2] * e2[1],
         e1[2] * e2[0] - e1[0] * e2[2],
         e1[0] * e2[1] - e1[1] * e2[0])
    ln = math.sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2])
    if ln < 1e-9:
        return None
    return (n[0] / ln, n[1] / ln, n[2] / ln)


def depth_bias(path):
    lines = open(path, encoding="utf-8").read().splitlines(keepends=True)
    verts = []          # (x,y,z)
    vline = []          # index of the source line for vertex k
    faces = []          # dict: vidx list, obj index, kind, normal, plane offset d
    cur_obj = -1
    cur_kind = "cap"
    for li, ln in enumerate(lines):
        p = ln.split()
        if not p:
            continue
        if p[0] == "o":
            cur_obj += 1
            cur_kind = "rom" if "_rom_" in ln else "cap"
        elif p[0] == "v":
            verts.append((float(p[1]), float(p[2]), float(p[3])))
            vline.append(li)
        elif p[0] == "f":
            idx = [int(t.split("/")[0]) - 1 for t in p[1:]]
            vs = [verts[i] for i in idx]
            n = _face_normal(vs)
            if n is None:
                continue
            d = -(n[0] * vs[0][0] + n[1] * vs[0][1] + n[2] * vs[0][2])
            cen = (sum(v[0] for v in vs) / len(vs),
                   sum(v[1] for v in vs) / len(vs),
                   sum(v[2] for v in vs) / len(vs))
            pos = n[0] * cen[0] + n[1] * cen[1] + n[2] * cen[2]   # along outward normal
            dom = max(range(3), key=lambda k: abs(n[k]))          # plane-normal axis
            ax = [k for k in range(3) if k != dom]                # the two in-plane axes
            bb = (min(v[ax[0]] for v in vs), max(v[ax[0]] for v in vs),
                  min(v[ax[1]] for v in vs), max(v[ax[1]] for v in vs))
            faces.append({"idx": idx, "obj": cur_obj, "kind": cur_kind,
                          "n": n, "d": d, "pos": pos, "bb": bb, "fi": len(faces)})

    # bucket faces by quantised plane (orient normal to a canonical hemisphere for the key)
    buckets = {}
    for f in faces:
        n = f["n"]
        s = 1.0
        for c in n:                      # canonical hemisphere: first non-zero component > 0
            if abs(c) > 1e-6:
                s = 1.0 if c > 0 else -1.0
                break
        key = (round(n[0] * s / NQ), round(n[1] * s / NQ), round(n[2] * s / NQ),
               round(f["d"] * s / DQ))
        buckets.setdefault(key, []).append(f)

    def fronter(a, b):
        """Is object a in front of object b? Captured in front of recovered; among the
        same kind, the later-drawn (higher index) in front (MAME painter order)."""
        return (akind[a], a) > (akind[b], b)

    def overlap2d(ba, bb):   # in-plane boxes touch/overlap (gap below GAP in both axes)
        return (min(ba[1], bb[1]) - max(ba[0], bb[0]) > -GAP and
                min(ba[3], bb[3]) - max(ba[2], bb[2]) > -GAP)

    def area_overlap(ba, bb):   # real in-plane AREA intersection (abutting tiles excluded)
        return (min(ba[1], bb[1]) - max(ba[0], bb[0]) > OVM and
                min(ba[3], bb[3]) - max(ba[2], bb[2]) > OVM)

    akind = {}
    for f in faces:
        akind.setdefault(f["obj"], 1 if f["kind"] == "cap" else 0)

    # Coplanar depth levels: within a coarse bucket, only faces at the SAME depth (within
    # TIE) actually z-fight. Collect every level across all buckets so the two passes below
    # can see a base and the decal painted on it together.
    all_levels = []
    for key, grp in buckets.items():
        levels = {}
        for f in grp:
            levels.setdefault(round(f["pos"] / TIE), []).append(f)
        all_levels.extend(levels.values())

    fo = [0.0] * len(faces)   # per-face signed nudge, measured along the face's own +normal

    # Pass A -- BETWEEN objects (historical rule, unchanged): push a base back along
    # -normal, one layer per distinct front-er object whose in-plane box overlaps it. The
    # front-most overlapping face stays put; a face with no front-er overlapper is never
    # moved, so the central stone slab keeps its height and merely-abutting floor tiles are
    # left tiling. Only groups spanning two or more objects take part.
    for lgrp in all_levels:
        if len(set(f["obj"] for f in lgrp)) < 2:
            continue
        for f in lgrp:
            front_objs = set()
            for g in lgrp:
                if g["obj"] != f["obj"] and fronter(g["obj"], f["obj"]) \
                        and overlap2d(f["bb"], g["bb"]):
                    front_objs.add(g["obj"])
            if front_objs:
                fo[f["fi"]] += -EPS * len(front_objs)

    # Pass B -- WITHIN an object: a face that AREA-overlaps an earlier-drawn face of the
    # same object is a decal painted onto that base (the banner glyph on its brick header),
    # and must sit in front of it. The geometriser does not wind decor faces to a consistent
    # side, so moving the base along +-normal is not reliably "away from the viewer" (and
    # pulls in the abutting-record seam Pass A leaves between the banner's two halves).
    # Instead lift the decal itself TOWARD THE ORIGIN -- the arena interior, hence the
    # viewer, since the stages are feet-centred with decal walls facing in -- to just in
    # front of the base's *committed* position. Reading Pass A's result and walking the
    # level in paint order makes the decal clear its base even when Pass A moved the base,
    # and stacks a decal-on-decal correctly; it is symmetric for both banner halves and
    # works for floors (viewer above) and ceilings (viewer below) alike.
    for lgrp in all_levels:
        for f in sorted(lgrp, key=lambda g: g["fi"]):
            bases = [g for g in lgrp if g["obj"] == f["obj"] and g["fi"] < f["fi"]
                     and area_overlap(f["bb"], g["bb"])]
            if not bases:
                continue
            t = -1.0 if f["pos"] > 0.0 else 1.0        # +n side that points toward origin
            need = max(t * fo[g["fi"]] for g in bases) + EPS
            if t * fo[f["fi"]] < need:
                fo[f["fi"]] = need * t

    # apply each face's along-normal nudge to its own vertices (faces never share vertices)
    off = [(0.0, 0.0, 0.0)] * len(verts)
    nudged = 0
    for f in faces:
        s = fo[f["fi"]]
        if s == 0.0:
            continue
        n = f["n"]
        dx, dy, dz = s * n[0], s * n[1], s * n[2]
        for i in f["idx"]:
            ox, oy, oz = off[i]
            off[i] = (ox + dx, oy + dy, oz + dz)
        nudged += 1

    # rewrite the v lines that moved
    for k, (dx, dy, dz) in enumerate(off):
        if dx == 0.0 and dy == 0.0 and dz == 0.0:
            continue
        x, y, z = verts[k]
        lines[vline[k]] = "v %.6f %.6f %.6f\n" % (x + dx, y + dy, z + dz)
    open(path, "w", encoding="utf-8").write("".join(lines))
    return nudged, len(faces)


if __name__ == "__main__":
    n, tot = depth_bias(sys.argv[1])
    print("  depth-bias: nudged %d / %d faces in coplanar groups" % (n, tot))
