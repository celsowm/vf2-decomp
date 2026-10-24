#!/usr/bin/env python3
"""Audit VF2 sound-ROM Timer-A register writes; never export proprietary bytes."""
from __future__ import annotations
import argparse
import json
import pathlib
import sys
import zipfile

CHIP = "epr-17574.30"
SOUND_ROM_BYTES = 0x80000
SAMPLE_RATE = 44100
VECTOR_L2 = 26
BOOT_TIMA_PC = 0x02F4
L2_TIMA_PC = 0x1F1A
L2_IRQ_ENABLE_PC = 0x1F20
M68K_MOVE_IMM_WORD_DISP_A5 = 0x3B7C


def _sound_chip_bytes(source: pathlib.Path) -> bytes:
    if source.is_dir():
        return (source / CHIP).read_bytes()
    with zipfile.ZipFile(source) as archive:
        matches = [name for name in archive.namelist()
                   if pathlib.PurePosixPath(name).name.lower() == CHIP]
        if len(matches) != 1:
            raise ValueError("expected exactly one VF2 sound chip")
        return archive.read(matches[0])


def normalize_sound_rom(chip: bytes) -> bytes:
    """Undo manifest LOAD16_WORD_SWAP to see original 68000 big-endian words."""
    if len(chip) != SOUND_ROM_BYTES:
        raise ValueError("unexpected sound chip length")
    result = bytearray(SOUND_ROM_BYTES)
    result[::2], result[1::2] = chip[1::2], chip[::2]
    return bytes(result)


def _word(rom: bytes, pc: int) -> int:
    return int.from_bytes(rom[pc:pc + 2], "big")


def _assert_move_immediate(rom: bytes, pc: int, address: int) -> int:
    if (_word(rom, pc) != M68K_MOVE_IMM_WORD_DISP_A5 or
            _word(rom, pc + 4) != address):
        raise ValueError(f"unexpected M68000 timer instruction at 0x{pc:04X}")
    return _word(rom, pc + 2)


def audit_sound_rom(chip: bytes) -> dict[str, int | str | bool]:
    rom = normalize_sound_rom(chip)
    vector = int.from_bytes(rom[VECTOR_L2 * 4:VECTOR_L2 * 4 + 4], "big")
    if vector != 0x601F16:
        raise ValueError("L2 vector 26 does not target 0x601F16")
    boot_tima = _assert_move_immediate(rom, BOOT_TIMA_PC, 0x0418)
    isr_tima = _assert_move_immediate(rom, L2_TIMA_PC, 0x0418)
    scieb = _assert_move_immediate(rom, L2_IRQ_ENABLE_PC, 0x0422)
    if boot_tima != isr_tima or (scieb & 0x40) == 0:
        raise ValueError("boot/ISR reload mismatch or Timer-A interrupt disabled")
    if boot_tima & 0xF800:
        raise ValueError("unmodeled reserved timer bits")
    reload = boot_tima & 0xFF
    prescaler = 1 << ((boot_tima >> 8) & 7)
    if reload == 0xFF:
        raise ValueError("Timer A 0xFF has no finite interval")
    frames = (0xFF - reload) * prescaler
    return {
        "chip": CHIP, "l2_vector": f"0x{vector:06x}",
        "boot_tima": f"0x{boot_tima:04x}",
        "isr_tima": f"0x{isr_tima:04x}",
        "isr_scieb": f"0x{scieb:04x}",
        "prescaler_frames": prescaler,
        "timer_a_period_sample_frames": frames,
        "sample_rate_hz": SAMPLE_RATE,
        "timer_a_period_us": round(frames * 1_000_000 / SAMPLE_RATE, 6),
        "timer_a_rate_hz": round(SAMPLE_RATE / frames, 6),
        "ten_seconds_irq_opportunities": (10 * SAMPLE_RATE) // frames,
        "ten_seconds_exact_sample_multiple": ((10 * SAMPLE_RATE) % frames) == 0,
        "irq_delivery_or_cpu_cycles_verified": False,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=pathlib.Path,
                        help="Private VF2 ZIP archive or extracted ROM folder")
    args = parser.parse_args(argv)
    try:
        result = audit_sound_rom(_sound_chip_bytes(args.source))
    except (OSError, ValueError, zipfile.BadZipFile) as exc:
        print(f"SCSP_TIMA_REJECT: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
