#!/usr/bin/env python3
"""Render measured Model 2 tile-plane text from a .vf2snap (host-side view)."""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "out"))
from dump_snap_video import read_snapshot  # noqa: E402

COLS = 64
ROWS = 48


def tile_char(value: int) -> str:
    if (value & 0xFF00) == 0x8000:
        ch = value & 0xFF
        if 32 <= ch < 127:
            return chr(ch)
        return "?"
    if value == 0:
        return " "
    return "."


def render_text(tile: bytes) -> str:
    lines = []
    for row in range(ROWS):
        chars = []
        for col in range(COLS):
            off = (row * COLS + col) * 2
            if off + 1 >= len(tile):
                chars.append(" ")
                continue
            value = tile[off] | (tile[off + 1] << 8)
            chars.append(tile_char(value))
        lines.append("".join(chars).rstrip())
    return "\n".join(lines) + "\n"


def render_ppm(tile: bytes, path: Path) -> None:
    """Coarse host visualization: 8x8 blocks, white text on black."""
    scale = 4
    width = COLS * scale
    height = ROWS * scale
    pixels = bytearray(width * height * 3)
    for row in range(ROWS):
        for col in range(COLS):
            off = (row * COLS + col) * 2
            if off + 1 >= len(tile):
                continue
            value = tile[off] | (tile[off + 1] << 8)
            on = (value & 0xFF00) == 0x8000 and 32 <= (value & 0xFF) < 127
            color = (240, 240, 240) if on else (16, 16, 24)
            for y in range(scale):
                for x in range(scale):
                    px = ((row * scale + y) * width + (col * scale + x)) * 3
                    pixels[px : px + 3] = bytes(color)
    header = f"P6\n{width} {height}\n255\n".encode("ascii")
    path.write_bytes(header + pixels)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("snapshot")
    parser.add_argument("--text-out", type=Path)
    parser.add_argument("--ppm-out", type=Path)
    args = parser.parse_args()
    snap = read_snapshot(Path(args.snapshot))
    tile = snap["regions"]["tile-ram"]
    text = render_text(tile)
    if args.text_out:
        args.text_out.write_text(text, encoding="utf-8")
        print(f"wrote {args.text_out}")
    else:
        sys.stdout.write(text)
    if args.ppm_out:
        render_ppm(tile, args.ppm_out)
        print(f"wrote {args.ppm_out}")


if __name__ == "__main__":
    main()
