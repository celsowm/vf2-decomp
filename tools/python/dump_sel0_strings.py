from pathlib import Path
import struct

ROM_DIR = Path("roms/vf2")
MAIN_SIZE = 0x200000

def build_maincpu():
    region = bytearray(MAIN_SIZE)
    pairs = (
        ("epr-18385.12", 0x00000000),
        ("epr-18386.13", 0x00000002),
        ("epr-18387.14", 0x00040000),
        ("epr-18388.15", 0x00040002),
    )
    for name, off in pairs:
        src = (ROM_DIR / name).read_bytes()
        for i in range(0, len(src), 2):
            di = off + i * 2
            if di + 1 < MAIN_SIZE:
                region[di] = src[i]
                region[di + 1] = src[i + 1]
    return bytes(region)

img = build_maincpu()

def read_cstr(addr, n=80):
    off = addr  # main ROM base 0
    if off < 0 or off >= len(img):
        return f"<oob {addr:08x}>"
    raw = img[off:off+n]
    # try ASCII
    s = []
    for b in raw:
        if 32 <= b < 127:
            s.append(chr(b))
        elif b == 0:
            break
        else:
            s.append(f"\\x{b:02x}")
            if len(s) > 40:
                break
    return "".join(s)

def read_tilestr(addr, n=64):
    """0x80xx LE halfwords → chars"""
    off = addr
    chars = []
    for i in range(0, n, 2):
        if off+i+1 >= len(img):
            break
        v = img[off+i] | (img[off+i+1] << 8)
        if (v & 0xFF00) == 0x8000 and 32 <= (v & 0xFF) < 127:
            chars.append(chr(v & 0xFF))
        elif v == 0:
            chars.append(" ")
        else:
            chars.append(".")
    return "".join(chars)

addrs = [0xA9A4, 0xA9B2, 0xA9D9, 0xA9FA, 0xAA1A, 0xAA42, 0xAA67, 0xAA87, 0xAAAD]
print("=== ASCII/tile strings used by selector-0 signature blits ===")
for a in addrs:
    print(f"  {a:08x}: cstr={read_cstr(a)!r}")
    print(f"           tile={read_tilestr(a, 48)!r}")
    print(f"           hex={img[a:a+24].hex()}")
