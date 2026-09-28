#!/usr/bin/env python3
"""Stable decode/import surface for host scene reconstruction tools.

ANALYSIS helper — not recovered game C, not a logo/title witness.

Import geometry decode from a stable path so scene tools keep working even if
render_mesh_host.py is extended by parallel agents (P1/P2). Prefer importing
from render_mesh_host when available; fall back to a local tgp.c-aligned
skip3/noskip implementation otherwise.

Object table (measured):
  TABLE 0x020E0004, 16-byte records, word_index = w2 & 0x7fffff,
  poly byte = word_index * 4. Pairs in render_poly_objects.py.
"""
from __future__ import annotations

import math
import struct
import sys
from pathlib import Path
from typing import Any, Iterable  # Any used by protocol_tag_decode / tables

sys.path.insert(0, str(Path(__file__).resolve().parent))

from render_poly_objects import (  # noqa: E402
    BASE,
    MAIN_PAIRS,
    POLY_PAIRS,
    TABLE,
    build,
    f32,
    u32,
)

MAIN_DATA_BIN = Path("out/main_data.bin")
DEFAULT_ROM_DIR = Path("roms/vf2")
TABLE_LO = 0x020E0000
TABLE_HI = 0x020F0000
FIFO_LO = 0x00884000
FIFO_HI = 0x00886000
GEO_LO = 0x00800000
GEO_HI = 0x00810000
GEO_PORT_ADDRS = frozenset({0x00800010, 0x00804000})


def import_decode_object():
    """Return decode_object(poly, word_index, skip_words=..., ...) API.

    Prefers render_mesh_host.decode_object (may gain features from P1).
    Falls back to local implementation if that import fails.
    """
    try:
        import render_mesh_host as rmh

        fn = getattr(rmh, "decode_object", None)
        if callable(fn):
            return fn
    except Exception:
        pass
    return local_decode_object


def local_decode_object(
    poly: bytes,
    word_index: int,
    *,
    skip_words: int = 3,
    max_links: int = 4096,
    max_tris: int = 12000,
):
    """tgp.c geometry_execute_object float-link decode (skip3|noskip).

    Returns (tris, nonfinite_rejected, truncated).
    Each tri is ((x,y,z), (x,y,z), (x,y,z)).
    """
    tris: list[tuple] = []
    nonfinite_rejected = 0
    truncated = False
    off = word_index * 4

    def rd() -> int:
        nonlocal off
        if off < 0 or off + 4 > len(poly):
            raise EOFError
        v = u32(poly, off)
        off += 4
        return v

    try:
        p0 = (f32(rd()), f32(rd()), f32(rd()))
        p1 = (f32(rd()), f32(rd()), f32(rd()))
    except EOFError:
        return tris, nonfinite_rejected, truncated

    for _ in range(max_links):
        try:
            attr = rd()
        except EOFError:
            break
        if (attr & 3) == 0:
            break
        p3 = None
        try:
            for _k in range(skip_words):
                rd()
            p2 = (f32(rd()), f32(rd()), f32(rd()))
            if attr & 1:
                p3 = (f32(rd()), f32(rd()), f32(rd()))
        except EOFError:
            break
        pts = p0 + p1 + p2 + (p3 or ())
        if all(math.isfinite(c) for c in pts):
            tris.append((p0, p1, p2))
            if p3 is not None:
                tris.append((p0, p2, p3))
            if len(tris) >= max_tris:
                truncated = True
                break
        else:
            nonfinite_rejected += 1
        mode = (attr >> 8) & 3
        if mode in (0, 2):
            p0 = p2
            p1 = p3 if p3 is not None else p2
        elif mode == 1:
            p1 = p2
        else:
            p0 = p3 if p3 is not None else p2
    return tris, nonfinite_rejected, truncated


def load_object_table(
    main_img: bytes | None = None,
    *,
    main_data_path: Path | None = None,
    rom_dir: Path | None = None,
    max_id: int = 0x2000,
) -> dict[str, Any]:
    """Build id→record and w0→id reverse maps from measured main_data.

    Prefer prebuilt out/main_data.bin; else rebuild from roms/vf2 pairs.
    Fails closed on missing ROM: tables empty, callers must not invent ids.
    """
    if main_img is None:
        path = main_data_path or MAIN_DATA_BIN
        if path.is_file():
            main_img = path.read_bytes()
        else:
            rd = rom_dir or DEFAULT_ROM_DIR
            import render_poly_objects as rpo

            prev = rpo.ROM_DIR
            rpo.ROM_DIR = rd
            try:
                main_img = build(MAIN_PAIRS, 0x02400000)
            finally:
                rpo.ROM_DIR = prev

    by_id: dict[int, dict[str, Any]] = {}
    w0_to_id: dict[int, int] = {}
    id_to_w0: dict[int, int] = {}
    for obj_id in range(max_id):
        off = (TABLE - BASE) + obj_id * 16
        if off + 16 > len(main_img):
            break
        w0, w1, w2, w3 = struct.unpack_from("<IIII", main_img, off)
        if w0 == 0 and w1 == 0 and w2 == 0 and w3 == 0:
            continue
        word_index = w2 & 0x7FFFFF
        rec = {
            "id": obj_id,
            "w0": w0,
            "w1": w1,
            "w2": w2,
            "w3": w3,
            "word_index": word_index,
            "poly_byte": word_index * 4,
            "poly_rom": bool(w2 & 0x00800000),
        }
        by_id[obj_id] = rec
        if w0:
            id_to_w0[obj_id] = w0
            # First id wins on collision (same convention as extract_fifo_transforms).
            w0_to_id.setdefault(w0, obj_id)
    return {
        "by_id": by_id,
        "w0_to_id": w0_to_id,
        "id_to_w0": id_to_w0,
        "main_len": len(main_img) if main_img else 0,
        "source": str(main_data_path or MAIN_DATA_BIN),
    }


def load_poly(rom_dir: Path | None = None, poly_path: Path | None = None) -> bytes:
    if poly_path is not None and poly_path.is_file():
        return poly_path.read_bytes()
    rd = rom_dir or DEFAULT_ROM_DIR
    import render_poly_objects as rpo

    prev = rpo.ROM_DIR
    rpo.ROM_DIR = rd
    try:
        return build(POLY_PAIRS, 0x02000000)
    finally:
        rpo.ROM_DIR = prev


def u32le(hex_bytes: str | None) -> int:
    if not hex_bytes:
        return 0
    try:
        bs = bytes.fromhex(hex_bytes)[:4]
    except ValueError:
        return 0
    if not bs:
        return 0
    return int.from_bytes(bs, "little")


def classify_port(addr: int) -> str | None:
    if FIFO_LO <= addr < FIFO_HI:
        return "fifo"
    if addr in GEO_PORT_ADDRS:
        return "geo_port"
    if GEO_LO <= addr < GEO_HI:
        return "geo"
    return None


def is_protocol_color_like(val: int) -> bool:
    """FIFO color/tag immediates — analysis fills, NOT proven RGB.

    Measured family (phase5 attract, helper 0x7c60 path):
      0x00800101, 0x01800303, 0x03000606, 0x07800f0f, 0x09801313,
      0x14802929, 0x1a003434, 0x1c803939, 0x33806767, 0x34806969,
      0x35806b6b, 0x36006c6c.

    Byte layout AA BB CC DD:
      - CC == DD (YY=ZZ intensity hypothesis) required
      - BB in {0x00, 0x80} (protocol class marker in byte2)
      - CC >= 1 (rejects 0x00000000 / 0x80000000 / 0x3f800000 float words
        where low two bytes are 0)
      - AA may be 0x00 (e.g. 0x00800101) — do NOT require AA != 0
    """
    if val == 0 or val == 0xFFFFFFFF:
        return False
    b0 = val & 0xFF
    b1 = (val >> 8) & 0xFF
    b2 = (val >> 16) & 0xFF
    if b0 != b1 or b0 < 1:
        return False
    return b2 in (0x00, 0x80)


def protocol_tag_decode(val: int) -> dict[str, Any] | None:
    """Decode one measured FIFO protocol word (confidence=protocol_tag).

    Hypothesis ONLY (NOT game-accurate palette / NOT CRT RGB):
      word bytes AA BB CC DD
      tag_high = AA<<8 | BB   (e.g. 0x1480, 0x0080, 0x0300)
      intensity = CC when CC==DD
      hue seed comes from tag_high so sibling tags stay distinguishable
      in host analysis fills; intensity scales value.
    Always return confidence=protocol_tag.
    """
    if not is_protocol_color_like(val):
        return None
    aa = (val >> 24) & 0xFF
    bb = (val >> 16) & 0xFF
    cc = (val >> 8) & 0xFF
    dd = val & 0xFF
    tag_high = (aa << 8) | bb
    yy_eq_zz = cc == dd
    intensity = cc if yy_eq_zz else min(cc, dd)
    if yy_eq_zz:
        hypothesis = (
            f"YY=ZZ=0x{cc:02x} intensity; high 0x{tag_high:04x} is protocol "
            f"tag (hue seed in analysis fill), not a recovered RGB channel"
        )
    else:
        hypothesis = (
            f"CC=0x{cc:02x} DD=0x{dd:02x} (not equal); high 0x{tag_high:04x} "
            f"is protocol tag"
        )
    return {
        "word": f"0x{val:08x}",
        "word_int": val,
        "bytes_be": [aa, bb, cc, dd],
        "tag_high": f"0x{tag_high:04x}",
        "yy": f"0x{cc:02x}",
        "zz": f"0x{dd:02x}",
        "yy_eq_zz": yy_eq_zz,
        "intensity": intensity,
        "hypothesis": hypothesis,
        "confidence": "protocol_tag",
        "game_accurate_palette": False,
        "crt_claim": False,
    }


def protocol_tag_rgb(val: int) -> tuple[int, int, int] | None:
    """Map analysis FIFO tag word → host fill RGB (NOT game color).

    Every use is confidence=protocol_tag. Hue is seeded from tag_high and
    value from intensity so sibling protocol tags stay distinguishable in
    multi-object host sheets. Never claim CRT/framebuffer or recovered RGB.
    """
    meta = protocol_tag_decode(val)
    if meta is None:
        return None
    tag_high = int(meta["tag_high"], 16)
    intensity = int(meta["intensity"])
    # Deterministic hue from protocol tag; value from measured intensity.
    h = ((tag_high * 37) % 360) / 360.0
    s = 0.62
    v = 0.22 + 0.70 * min(1.0, intensity / 127.0)
    r, g, b = _hsv_to_rgb(h, s, v)
    return (
        max(24, min(240, r)),
        max(24, min(240, g)),
        max(24, min(240, b)),
    )


def iter_jsonl(path: Path):
    with path.open("r", errors="replace") as fh:
        for line in fh:
            if not line.startswith("{"):
                continue
            try:
                import json

                yield json.loads(line)
            except Exception:
                continue


def index_palette(ids: Iterable[int]) -> dict[int, tuple[int, int, int]]:
    """Deterministic per-id analysis palette (distinct from FIFO tag fills)."""
    # Golden-angle hue walk + fixed V/S for stable multi-object legibility.
    palette: dict[int, tuple[int, int, int]] = {}
    for obj_id in ids:
        h = ((obj_id * 137) % 360) / 360.0
        s, v = 0.65, 0.90
        r, g, b = _hsv_to_rgb(h, s, v)
        # Avoid pure black / pure white fills.
        palette[obj_id] = (
            max(28, min(240, r)),
            max(28, min(240, g)),
            max(28, min(240, b)),
        )
    return palette


def _hsv_to_rgb(h: float, s: float, v: float) -> tuple[int, int, int]:
    i = int(h * 6.0) % 6
    f = h * 6.0 - int(h * 6.0)
    p = v * (1 - s)
    q = v * (1 - f * s)
    t = v * (1 - (1 - f) * s)
    if i == 0:
        r, g, b = v, t, p
    elif i == 1:
        r, g, b = q, v, p
    elif i == 2:
        r, g, b = p, v, t
    elif i == 3:
        r, g, b = p, q, v
    elif i == 4:
        r, g, b = t, p, v
    else:
        r, g, b = v, p, q
    return int(r * 255), int(g * 255), int(b * 255)


def try_import_pil():
    try:
        from PIL import Image, ImageDraw, ImageFont  # type: ignore

        return Image, ImageDraw, ImageFont
    except Exception:
        return None, None, None
