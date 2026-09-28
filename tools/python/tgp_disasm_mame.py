#!/usr/bin/env python3
"""Analysis-only MB86233/TGP disassembler inspired by MAME mame0289 mb86233d.

Reads a little-endian u32 program image (e.g. extracted copro_data or a
snapshot program buffer) and prints a best-effort listing. This tool does
not execute TGP code and is not part of the recovered runtime.
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

# Register names from MAME mb86233d.cpp (BSD-3-Clause reference).
REGS = [
    "b0", "b1", "x0", "x1", "x2", "i0", "i1", "i2",
    "sp", "pag", "vsm", "dmc", "c0", "c1", "pc", "-",
    "a", "ah", "al", "b", "bh", "bl", "c", "ch", "cl",
    "d", "dh", "dl", "p", "ph", "pl", "sft",
]
for i in range(16):
    REGS.append(f"rf{i:x}")
REGS += ["sio0", "si1", "pio", "pioa", "rpc", "r35", "r36", "r37",
         "pad", "mod", "ear", "st", "mask", "tim", "cx", "dx"]

ALU = {
    0x00: None,
    0x01: "andd",
    0x02: "orad",
    0x03: "eord",
    0x04: "notd",
    0x05: "fcpd",
    0x06: "fadd",
    0x07: "fsbd",
    0x08: "fml",
    0x09: "fmsd",
    0x0A: "fmrd",
    0x0B: "fabd",
    0x0C: "fsmd",
    0x0D: "fspd",
    0x0E: "cxfd",
    0x0F: "cfxd",
    0x10: "fdvd",
    0x11: "fned",
    0x13: "d=b+a",
    0x14: "d=b-a",
    0x16: "lsrd",
    0x17: "lsld",
    0x18: "asrd",
    0x19: "asld",
    0x1A: "addd",
    0x1B: "subd",
}

COND = {
    0x00: "zrd",
    0x01: "ged",
    0x02: "led",
    0x0A: "gpio0",
    0x0B: "gpio1",
    0x0C: "gpio2",
    0x10: "zc0",
    0x11: "zc1",
    0x12: "gpio3",
    0x16: "alw",
}


def reg(n: int) -> str:
    n &= 0x3F
    return REGS[n] if n < len(REGS) else f"r{n:02x}"


def mem_addr(n: int) -> str:
    n &= 0x1FF
    if n < 0x80:
        return f"${n:02x}"
    if n < 0xC0:
        return f"${n & 0x7f:02x}(x0)"
    if n < 0x100:
        return f"${n & 0x7f:02x}(x0+)"
    # remaining encodings abbreviated
    return f"mem({n:03x})"


def disasm_word(opc: int, pc: int) -> str:
    group = (opc >> 26) & 0x3F
    alu = (opc >> 21) & 0x1F
    r1 = opc & 0x1FF
    r2 = (opc >> 9) & 0x1FF
    alu_s = ALU.get(alu, f"alu{alu:02x}") or ""
    if group == 0x00:
        return f"lab/mov  {mem_addr(r2)} -> {mem_addr(r1)} {alu_s}".rstrip()
    if group == 0x07:
        return f"ld/mov   {mem_addr(r1)} <-> {mem_addr(r2)} {alu_s}".rstrip()
    if group in (0x0D,):
        sub2 = (opc >> 17) & 7
        return f"stm/clm  sub2={sub2} raw={opc:08x}"
    if group in (0x0E,):
        inst = ["lipl", "lia", "lib", "lid"][(opc >> 24) & 3]
        return f"{inst} #0x{opc & 0xFFFFFF:06x}"
    if group == 0x0F:
        alu_f = (opc >> 20) & 0x1F
        sub2 = (opc >> 17) & 7
        return f"rep/clr/set alu={alu_f:02x} sub2={sub2} raw={opc:08x}"
    if 0x10 <= group <= 0x1F:
        return f"ldi #0x{opc & 0xFFFFFF:06x}, {reg((opc >> 24) & 0x3F)}"
    if group in (0x2F, 0x3F):
        cond = (opc >> 20) & 0x1F
        subtype = (opc >> 17) & 7
        invert = "!" if opc & 0x40000000 else ""
        cs = COND.get(cond, f"cond{cond:02x}")
        data = opc & 0xFFFF
        return f"br/ldif/rtif {invert}{cs} sub={subtype} data=0x{data:04x}"
    return f"unk group=0x{group:02x} raw={opc:08x}"


def load_words(path: Path, limit: int, offset: int) -> list[int]:
    data = path.read_bytes()
    words = []
    for i in range(offset, min(len(data), offset + limit * 4), 4):
        if i + 4 > len(data):
            break
        words.append(struct.unpack_from("<I", data, i)[0])
    return words


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("image", type=Path, help="LE u32 TGP program image")
    ap.add_argument("--base", type=int, default=0, help="word/byte base offset")
    ap.add_argument("--count", type=int, default=64, help="number of words")
    ap.add_argument("--bytes", action="store_true", help="--base is byte offset")
    args = ap.parse_args()
    off = args.base if args.bytes else args.base * 4
    words = load_words(args.image, args.count, off)
    print(f"; {args.image} words@byte 0x{off:x} count={len(words)}")
    print("; MAME-inspired analysis listing — not an oracle")
    for i, w in enumerate(words):
        addr = (off // 4) + i
        print(f"  {addr:04x}: {w:08x}  {disasm_word(w, addr)}")


if __name__ == "__main__":
    main()
