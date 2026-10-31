#!/usr/bin/env python3
"""Compare two existing PCM WAV artifacts exactly, without distributing audio.

This is an *artifact* parity check, not proof of independent producer
provenance, correct timer scheduling or arcade DAC fidelity. In particular,
identical native/oracle-labeled files may have been copied or mislabeled.

Usage: python tools/python/audio_pcm_diff.py native.wav original.wav --json
Exit 0: exact PCM and format match; 1: mismatch; 2: bad input.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import wave
from pathlib import Path

CHUNK_FRAMES = 8192


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def compare_pcm(first: Path, second: Path) -> dict:
    """Return a structured comparison; reject mismatched WAV format first."""
    with wave.open(str(first), "rb") as a, wave.open(str(second), "rb") as b:
        metadata_a = (a.getnchannels(), a.getsampwidth(), a.getframerate(),
                      a.getnframes(), a.getcomptype())
        metadata_b = (b.getnchannels(), b.getsampwidth(), b.getframerate(),
                      b.getnframes(), b.getcomptype())
        result = {
            "status": "match",
            "channels": metadata_a[0],
            "sample_width_bytes": metadata_a[1],
            "sample_rate": metadata_a[2],
            "frames": metadata_a[3],
            "pcm_sha256_first": None,
            "pcm_sha256_second": None,
            "first_different_frame": None,
            "first_different_channel": None,
            "first_different_byte_offset": None,
        }
        if metadata_a[4] != "NONE" or metadata_b[4] != "NONE":
            raise ValueError("only uncompressed PCM WAV is supported")
        if metadata_a != metadata_b:
            result["status"] = "format_mismatch"
            result["other_format"] = {
                "channels": metadata_b[0],
                "sample_width_bytes": metadata_b[1],
                "sample_rate": metadata_b[2],
                "frames": metadata_b[3],
            }
        else:
            first_hash = hashlib.sha256()
            second_hash = hashlib.sha256()
            bytes_per_frame = metadata_a[0] * metadata_a[1]
            frames_seen = 0
            while frames_seen < metadata_a[3]:
                size = min(CHUNK_FRAMES, metadata_a[3] - frames_seen)
                x = a.readframes(size)
                y = b.readframes(size)
                if len(x) != size * bytes_per_frame or len(y) != len(x):
                    raise ValueError("truncated WAV PCM data")
                first_hash.update(x)
                second_hash.update(y)
                if x != y and result["first_different_frame"] is None:
                    byte_index = next(i for i, (v, w) in enumerate(zip(x, y))
                                      if v != w)
                    result["first_different_frame"] = (
                        frames_seen + byte_index // bytes_per_frame
                    )
                    result["first_different_channel"] = (
                        (byte_index % bytes_per_frame) // metadata_a[1]
                    )
                    result["first_different_byte_offset"] = (
                        frames_seen * bytes_per_frame + byte_index
                    )
                    result["status"] = "pcm_mismatch"
                frames_seen += size
            result["pcm_sha256_first"] = first_hash.hexdigest()
            result["pcm_sha256_second"] = second_hash.hexdigest()
    # A different WAV container can hold the same PCM. Report both,
    # but a different container header must not claim a PCM difference.
    result["wav_sha256_first"] = _sha256_file(first)
    result["wav_sha256_second"] = _sha256_file(second)
    result["wav_file_bytes_identical"] = (
        result["wav_sha256_first"] == result["wav_sha256_second"]
    )
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("first", type=Path, help="first PCM WAV artifact")
    parser.add_argument("second", type=Path, help="second PCM WAV artifact")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    try:
        report = compare_pcm(args.first, args.second)
    except (OSError, EOFError, ValueError, wave.Error) as exc:
        print(f"audio-pcm-diff: refused: {exc}", file=sys.stderr)
        return 2
    if args.json:
        print(json.dumps(report, sort_keys=True))
    else:
        print("audio-pcm-diff: " + report["status"])
        print("frames={frames} rate={sample_rate} channels={channels} "
              "width={sample_width_bytes}".format(**report))
        print("PCM SHA256: {} / {}".format(
            report["pcm_sha256_first"], report["pcm_sha256_second"]))
        print("whole WAV files byte-identical: " +
              str(report["wav_file_bytes_identical"]).lower())
        if report["first_different_frame"] is not None:
            print("first mismatch: frame={} channel={} byte_offset={}".format(
                report["first_different_frame"],
                report["first_different_channel"],
                report["first_different_byte_offset"]))
    return 0 if report["status"] == "match" else 1


if __name__ == "__main__":
    raise SystemExit(main())
