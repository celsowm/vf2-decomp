#!/usr/bin/env python3
"""ROM-free tests of VF2 SCSP timer opcode signatures and sample intervals."""
from __future__ import annotations
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from audio_scsp_tima_rom import audit_sound_rom, normalize_sound_rom


def chip_from_be(rom: bytes) -> bytes:
    b = bytearray(rom)
    b[::2], b[1::2] = rom[1::2], rom[::2]
    return bytes(b)


def fixture(reload: int = 0xCE, enable: int = 0x40) -> bytes:
    rom = bytearray(0x80000)
    rom[26 * 4:26 * 4 + 4] = (0x601F16).to_bytes(4, "big")
    for pc, reg, value in [(0x2F4, 0x418, reload),
                           (0x1F1A, 0x418, reload),
                           (0x1F20, 0x422, enable)]:
        rom[pc:pc + 6] = b"\x3b\x7c" + value.to_bytes(2, "big") + reg.to_bytes(2, "big")
    return chip_from_be(rom)


class TimerARomTests(unittest.TestCase):
    def test_measured_vf2_configuration(self):
        info = audit_sound_rom(fixture())
        self.assertEqual(info["timer_a_period_sample_frames"], 49)
        self.assertEqual(info["timer_a_rate_hz"], 900)
        self.assertEqual(info["ten_seconds_irq_opportunities"], 9000)
        self.assertTrue(info["ten_seconds_exact_sample_multiple"])

    def test_prescaler_and_reload(self):
        self.assertEqual(audit_sound_rom(fixture(0x3CE))["timer_a_period_sample_frames"], 392)
        self.assertEqual(audit_sound_rom(fixture(0xFE))["timer_a_period_sample_frames"], 1)
        self.assertEqual(audit_sound_rom(fixture(0))["timer_a_period_sample_frames"], 255)

    def test_fail_closed(self):
        for bad in [fixture(0xFF), fixture(0x800), fixture(enable=0x00), b"\0"]:
            with self.assertRaises(ValueError):
                audit_sound_rom(bad)
        broken = bytearray(fixture())
        broken[0x1F1A] = 0xFF
        with self.assertRaises(ValueError):
            audit_sound_rom(bytes(broken))
        broken = bytearray(fixture())
        broken[0x69] = 0x20
        with self.assertRaises(ValueError):
            audit_sound_rom(bytes(broken))

    def test_swap_roundtrip(self):
        rom = bytes(range(256)) * 2048
        self.assertEqual(normalize_sound_rom(chip_from_be(rom)), rom)


if __name__ == "__main__":
    unittest.main()
