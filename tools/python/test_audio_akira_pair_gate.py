#!/usr/bin/env python3
"""ROM-free orchestration tests. Fake producer is clearly not game evidence."""

import subprocess
import tempfile
import unittest
import wave
from pathlib import Path
from unittest.mock import patch

from audio_akira_pair_gate import PairGateError, EXPECTED, run_pair


class PairGateTests(unittest.TestCase):
    def setUp(self):
        self.tempdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tempdir.cleanup)
        self.root = Path(self.tempdir.name)
        self.rom_dir = self.root / "fake-rom-not-real"
        self.rom_dir.mkdir()
        self.binary = self.root / "fake-binary"
        self.binary.write_text("not executable", encoding="utf-8")
        self.mode_calls = []
        self.force_mismatch = False
        self.force_missing_marker = False
        self.force_failure = False

    def mock_run(self, args, **kwargs):
        self.assertEqual(len(args), 4)
        self.assertTrue(kwargs.get("capture_output"))
        self.assertGreater(kwargs.get("timeout", 0), 0)
        self.assertIn(args[2], {x[0] for x in EXPECTED})
        self.mode_calls.append(args[2])
        index = 0 if args[2] == EXPECTED[0][0] else 1
        out = Path(args[3]) / EXPECTED[index][1]
        sample = bytes.fromhex("0100ffff")
        if index == 1 and self.force_mismatch:
            sample = bytes.fromhex("0200ffff")
        with wave.open(str(out), "wb") as handle:
            handle.setnchannels(2)
            handle.setsampwidth(2)
            handle.setframerate(44100)
            handle.writeframes(sample * 441000)
        stdout = "AKIRA_MUSIC_SEQUENCE_FINISH\n" + EXPECTED[index][2]
        if self.force_missing_marker and index == 1:
            stdout = "AKIRA_MUSIC_SEQUENCE_FINISH"
        return subprocess.CompletedProcess(
            args, 1 if self.force_failure else 0, stdout=stdout, stderr=""
        )

    def test_two_distinct_modes_with_equal_pcm(self):
        with patch("audio_akira_pair_gate.subprocess.run", side_effect=self.mock_run):
            report = run_pair(self.binary, self.rom_dir)
        self.assertEqual(self.mode_calls, [EXPECTED[0][0], EXPECTED[1][0]])
        self.assertEqual(report["status"], "match")
        self.assertTrue(report["wav_file_bytes_identical"])
        self.assertFalse(report["arcade_timing_or_dac_fidelity_proven"])
        self.assertEqual(report["frames"], 441000)

    def test_mismatch_is_preserved_not_falsely_matched(self):
        self.force_mismatch = True
        with patch("audio_akira_pair_gate.subprocess.run", side_effect=self.mock_run):
            report = run_pair(self.binary, self.rom_dir)
        self.assertEqual(report["status"], "pcm_mismatch")
        self.assertEqual(report["first_different_frame"], 0)

    def test_nonzero_producer_refused(self):
        self.force_failure = True
        with patch("audio_akira_pair_gate.subprocess.run", side_effect=self.mock_run):
            with self.assertRaises(PairGateError):
                run_pair(self.binary, self.rom_dir)
        self.assertEqual(len(self.mode_calls), 1)

    def test_missing_mode_marker_refused(self):
        self.force_missing_marker = True
        with patch("audio_akira_pair_gate.subprocess.run", side_effect=self.mock_run):
            with self.assertRaises(PairGateError):
                run_pair(self.binary, self.rom_dir)
        self.assertEqual(len(self.mode_calls), 2)

    def test_no_private_rom_directory(self):
        with self.assertRaises(PairGateError):
            run_pair(self.binary, self.root / "missing")

    def test_unbounded_timeout_refused(self):
        with self.assertRaises(PairGateError):
            run_pair(self.binary, self.rom_dir, timeout_seconds=0)


if __name__ == "__main__":
    unittest.main()
