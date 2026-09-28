from pathlib import Path
import sys
sys.path.insert(0, "out")
from dump_snap_video import read_snapshot

def decode_glyph(v):
    lo = v & 0xFF
    hi = (v >> 8) & 0xFF
    if hi in (0x80, 0x89, 0x8A, 0x8B) and 32 <= lo < 127:
        return chr(lo)
    if v == 0 or lo == 0:
        return " "
    return "."

path = Path(sys.argv[1])
snap = read_snapshot(path)
tile = snap["regions"]["tile-ram"]
work = snap["regions"]["work-ram"]
print("==", path.name, "sel", work[0x2a], "a4", hex(work[0xa4]))
dests = [
    (0x332, 48),
    (0x618, 64),
    (0x818, 64),
    (0xA18, 64),
    (0xC18, 64),
    (0xE18, 64),
    (0x1018, 64),
    (0x1218, 64),
    (0x1650, 64),
]
for d, n in dests:
    chars = []
    for i in range(0, n * 2, 2):
        off = d + i
        if off + 1 >= len(tile):
            break
        v = tile[off] | (tile[off + 1] << 8)
        chars.append(decode_glyph(v))
    print(f"  0x0100{d:04x}: {''.join(chars)}")

# full scan 0x80-0x8f styled ASCII
print("--- full styled scan ---")
run = []
runs = []
for i in range(0, min(len(tile), 0x4000) - 1, 2):
    v = tile[i] | (tile[i + 1] << 8)
    hi = (v >> 8) & 0xFF
    lo = v & 0xFF
    if hi >= 0x80 and 32 <= lo < 127:
        run.append(chr(lo))
    else:
        if len(run) >= 3:
            runs.append((i, "".join(run)))
        run = []
if len(run) >= 3:
    runs.append((0, "".join(run)))
for off, s in runs:
    print(f"  @{off:04x}: {s}")
