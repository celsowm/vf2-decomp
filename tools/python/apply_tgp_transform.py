#!/usr/bin/env python3
"""Apply measured TGP geometry transform (matrix * focus) to xyz points.

Stable API for Phase A host raster:

    from apply_tgp_transform import apply_transform
    projected = apply_transform(points, matrix16, focus_x, focus_y)

Semantics mirror src/hardware/tgp.c geometry_transform_point:
  result.x = (m[0]*x + m[4]*y + m[8]*z  + m[12]*w) * focus_x
  result.y = (m[1]*x + m[5]*y + m[9]*z  + m[13]*w) * focus_y
  result.z =  m[2]*x + m[6]*y + m[10]*z + m[14]*w
  result.w =  m[3]*x + m[7]*y + m[11]*z + m[15]*w

If matrix16 is None and no measured transform is present, this module does
NOT invent a camera. It returns points unchanged (identity passthrough) and
records confidence="absent" on the report when loading transforms_phase5.json.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Sequence

# Column-major 4x4, same layout as vf2_tgp_matrix.values in tgp.c.
IDENTITY = [
    1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0,
]


def apply_transform(
    points: Sequence[Sequence[float]],
    matrix16: Sequence[float] | None,
    focus_x: float | None,
    focus_y: float | None,
) -> list[tuple[float, float, float]]:
    """Apply matrix*focus like tgp.c. Stable signature for Phase A.

    points: iterable of (x,y,z) or (x,y,z,w). w defaults to 1.0.
    matrix16: 16 floats column-major, or None for measured-absent.
    focus_x/focus_y: measured focus, or None for measured-absent.

    Returns list of projected (x,y,z) after focus scale. When matrix or focus
    is None, those terms are treated as identity/1.0 — this is documented as
    measured-absent passthrough, not as recovered game camera.
    """
    if matrix16 is None:
        m = IDENTITY
    else:
        if len(matrix16) != 16:
            raise ValueError(f"matrix16 must have 16 floats, got {len(matrix16)}")
        m = [float(v) for v in matrix16]
    fx = 1.0 if focus_x is None else float(focus_x)
    fy = 1.0 if focus_y is None else float(focus_y)

    out: list[tuple[float, float, float]] = []
    for p in points:
        if len(p) == 3:
            x, y, z = float(p[0]), float(p[1]), float(p[2])
            w = 1.0
        elif len(p) == 4:
            x, y, z, w = float(p[0]), float(p[1]), float(p[2]), float(p[3])
        else:
            raise ValueError(f"point must be xyz or xyzw, got {p!r}")
        ox = (m[0] * x + m[4] * y + m[8] * z + m[12] * w) * fx
        oy = (m[1] * x + m[5] * y + m[9] * z + m[13] * w) * fy
        oz = m[2] * x + m[6] * y + m[10] * z + m[14] * w
        out.append((ox, oy, oz))
    return out


def load_live_transform(path: Path) -> dict[str, Any]:
    data = json.loads(path.read_text(encoding="utf-8"))
    live = data.get("live_guess") or {}
    return {
        "matrix": live.get("matrix"),
        "focus_x": live.get("focus_x"),
        "focus_y": live.get("focus_y"),
        "geometry_mode": live.get("geometry_mode"),
        "confidence": live.get("confidence", "absent"),
        "note": live.get("note", ""),
        "source": str(path),
    }


def apply_from_transforms_json(
    points: Sequence[Sequence[float]],
    transforms_path: Path | str,
) -> tuple[list[tuple[float, float, float]], dict[str, Any]]:
    live = load_live_transform(Path(transforms_path))
    projected = apply_transform(
        points, live["matrix"], live["focus_x"], live["focus_y"]
    )
    return projected, live


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--transforms",
        type=Path,
        default=Path("out/attr-render/transforms_phase5.json"),
        help="extracted transforms JSON (live_guess)",
    )
    ap.add_argument(
        "--points",
        type=str,
        default="0,0,0;1,0,0;0,1,0;0,0,1",
        help="semicolon-separated xyz triples",
    )
    args = ap.parse_args()
    pts = []
    for chunk in args.points.split(";"):
        chunk = chunk.strip()
        if not chunk:
            continue
        vals = [float(x) for x in chunk.split(",")]
        pts.append(vals)
    live = load_live_transform(args.transforms)
    projected = apply_transform(pts, live["matrix"], live["focus_x"], live["focus_y"])
    print(json.dumps({
        "transforms_source": live["source"],
        "confidence": live["confidence"],
        "note": live["note"],
        "geometry_mode": live["geometry_mode"],
        "focus_x": live["focus_x"],
        "focus_y": live["focus_y"],
        "matrix": live["matrix"],
        "input_points": pts,
        "projected_xyz": [list(p) for p in projected],
        "fail_closed": (
            "confidence=absent means passthrough identity/1.0 — NOT a recovered camera"
            if live["confidence"] == "absent"
            else "confidence=measured — values came from FIFO/geo memory events"
        ),
    }, indent=2))


if __name__ == "__main__":
    main()
