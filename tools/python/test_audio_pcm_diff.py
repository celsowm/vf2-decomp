#!/usr/bin/env python3
"""ROM-free contract tests for exact two-WAV PCM artifact comparisons."""

import json
import subprocess
import sys
import tempfile
import unittest
import wave
from pathlib import Path

from audio_pcm_diff import compare_pcm


class PcmDiffTests(unittest.TestCase):
    def setUp(self):
        self.tempdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tempdir.cleanup)
        self.root = Path(self.tempdir.name)

    def wav(self, name, samples, *, channels=2, rate=44100):
        path = self.root / name
        with wave.open(str(path), "wb") as handle:
            handle.setnchannels(channels)
            handle.setsampwidth(2)
            handle.setframerate(rate)
            handle.writeframes(samples)
        return path

    def test_identical_pcm_and_container(self):
        raw = bytes.fromhex("0100ffff 0200feff 0300fdff 0400fcff")
        a = self.wav("a.wav", raw)
        b = self.wav("b.wav", raw)
        report = compare_pcm(a, b)
        self.assertEqual(report["status"], "match")
        self.assertEqual(report["frames"], 4)
        self.assertEqual(report["channels"], 2)
        self.assertTrue(report["wav_file_bytes_identical"])
        self.assertEqual(report["pcm_sha256_first"],
                         report["pcm_sha256_second"])
        self.assertIsNone(report["first_different_frame"])

    def test_first_different_stereo_channel(self):
        raw = bytearray(bytes.fromhex("0100ffff 0200feff 0300fdff 0400fcff"))
        a = self.wav("a.wav", bytes(raw))
        raw[10] = 0xff  # frame 2, right channel, first byte
        b = self.wav("b.wav", bytes(raw))
        report = compare_pcm(a, b)
        self.assertEqual(report["status"], "pcm_mismatch")
        self.assertEqual(report["first_different_frame"], 2)
        self.assertEqual(report["first_different_channel"], 1)
        self.assertEqual(report["first_different_byte_offset"], 10)
        self.assertNotEqual(report["pcm_sha256_first"],
                            report["pcm_sha256_second"])

    def test_rate_mismatch(self):
        raw = bytes.fromhex("0100ffff")
        a = self.wav("a.wav", raw)
        b = self.wav("b.wav", raw, rate=48000)
        report = compare_pcm(a, b)
        self.assertEqual(report["status"], "format_mismatch")
        self.assertEqual(report["other_format"]["sample_rate"], 48000)

    def test_channel_mismatch(self):
        raw = bytes.fromhex("0100ffff")
        a = self.wav("a.wav", raw)
        b = self.wav("b.wav", raw, channels=1)
        self.assertEqual(compare_pcm(a, b)["status"], "format_mismatch")

    def test_frame_count_mismatch(self):
        a = self.wav("a.wav", bytes.fromhex("0100ffff"))
        b = self.wav("b.wav", bytes.fromhex("0100ffff 0200feff"))
        self.assertEqual(compare_pcm(a, b)["status"], "format_mismatch")

    def test_malformed_file_refused(self):
        a = self.wav("a.wav", bytes.fromhex("0100ffff"))
        b = self.root / "bad.wav"
        b.write_text("not a wav", encoding="utf-8")
        with self.assertRaises((wave.Error, EOFError)):
            compare_pcm(a, b)

    def test_cli_json_and_exit_status(self):
        raw = bytes.fromhex("0100ffff")
        a = self.wav("a.wav", raw)
        b = self.wav("b.wav", raw)
        script = Path(__file__).with_name("audio_pcm_diff.py")
        cmd = [sys.executable, str(script), str(a), str(b), "--json"]
        matched = subprocess.run(cmd, text=True, capture_output=True, check=False)
        self.assertEqual(matched.returncode, 0, matched.stderr)
        self.assertEqual(json.loads(matched.stdout)["status"], "match")
        self.wav("b.wav", bytes.fromhex("0200ffff"))
        different = subprocess.run(cmd, text=True, capture_output=True, check=False)
        self.assertEqual(different.returncode, 1, different.stderr)
        self.assertEqual(json.loads(different.stdout)["first_different_frame"], 0)


if __name__ == "__main__":
    unittest.main()
