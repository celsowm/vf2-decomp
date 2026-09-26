#!/usr/bin/env python3
"""Standalone tests for tools/python/liftkit_vf2.py."""

from __future__ import annotations

import os
import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from liftkit_vf2 import (  # noqa: E402
    LiftkitVf2Error,
    normalize_vf2_disassembly,
    resolve_liftkit_root,
    resolve_vf2i960,
    run_lift,
)


def test_normalize_listing_and_span() -> None:
    source = (
        "00000010  5c501e00  mov      0, r10\n"
        "00000014  12345678 9abcdef0  ?\n"
    )
    result = normalize_vf2_disassembly(source, expected_address=0x10)
    assert result.text == (
        "00000010: 5c501e00  mov      0, r10\n"
        "00000014: 12345678 9abcdef0  ?\n"
    )
    assert result.instruction_count == 2
    assert result.byte_length == 0x0C
    print("ok: normalize listing, preserve words and calculate span")


def test_normalize_rejects_malformed_or_unexpected_output() -> None:
    for source in ("not a disassembly\n", "00000010  mov 0, r1\n"):
        try:
            normalize_vf2_disassembly(source)
        except LiftkitVf2Error:
            pass
        else:
            raise AssertionError("malformed output was accepted")

    try:
        normalize_vf2_disassembly("00000010  5c501e00  mov 0, r1\n", expected_address=0x20)
    except LiftkitVf2Error:
        pass
    else:
        raise AssertionError("unexpected start address was accepted")
    print("ok: reject malformed and unexpected disassembly")


def test_dependency_discovery() -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        entrypoint = root / "liftkit" / "src" / "liftkit" / "__main__.py"
        entrypoint.parent.mkdir(parents=True)
        entrypoint.write_text("", encoding="utf-8")
        assert resolve_liftkit_root(root) == root.resolve()
        assert (
            resolve_liftkit_root(
                environ={"SEGAMODEL2_TOOLS_ROOT": str(root)}
            )
            == root.resolve()
        )

        binary = root / "build" / "Debug" / "vf2i960.exe"
        binary.parent.mkdir(parents=True)
        binary.write_bytes(b"test")
        assert resolve_vf2i960(search_root=root) == binary.resolve()
    print("ok: discover configured liftkit and local vf2i960")


def test_missing_dependency_fails_without_output() -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        output = root / "out"
        try:
            run_lift(
                rom_dir=root,
                address=0x18644,
                count=1,
                name="missing",
                out_dir=output,
                liftkit_root=root / "missing-liftkit",
                vf2i960=root / "missing-vf2i960.exe",
            )
        except LiftkitVf2Error:
            pass
        else:
            raise AssertionError("missing liftkit was silently accepted")
        assert not output.exists()
    print("ok: missing dependency fails closed without partial output")


def test_optional_smoke() -> None:
    if os.environ.get("VF2_LIFTKIT_SMOKE") != "1":
        print("skip: optional liftkit smoke (set VF2_LIFTKIT_SMOKE=1)")
        return

    rom_dir = Path(os.environ.get("VF2_LIFTKIT_ROM_DIR", "roms/vf2"))
    liftkit_root = os.environ.get("SEGAMODEL2_TOOLS_ROOT")
    if not rom_dir.is_dir() or not liftkit_root:
        print("skip: optional liftkit smoke (ROM or SEGAMODEL2_TOOLS_ROOT missing)")
        return

    with tempfile.TemporaryDirectory() as temporary:
        output = Path(temporary) / "out"
        first = run_lift(
            rom_dir=rom_dir,
            address=0x18644,
            count=80,
            name="vf2_18644",
            out_dir=output,
            liftkit_root=Path(liftkit_root),
        )
        second = run_lift(
            rom_dir=rom_dir,
            address=0x27B5C,
            count=128,
            name="vf2_27b5c",
            out_dir=output,
            liftkit_root=Path(liftkit_root),
        )
        assert first["instruction_count"] == 80
        assert second["instruction_count"] == 128
        for result in (first, second):
            manifest_path = output / result["manifest"]
            assert manifest_path.name == "manifest.json"
            assert json.loads(manifest_path.read_text(encoding="utf-8")) == result
            run_root = manifest_path.parent
            assert (run_root / result["liftkit"]["slice"]).is_file()
            generated = result["liftkit"]["outputs"]
            for key in ("ir", "abi", "scaffold_c"):
                assert generated[key] is not None
                assert (run_root / generated[key]).is_file()
        assert all(path.resolve().is_relative_to(output.resolve()) for path in output.rglob("*"))
    print("ok: optional liftkit smoke at 0x18644 and 0x27b5c")


def main() -> int:
    test_normalize_listing_and_span()
    test_normalize_rejects_malformed_or_unexpected_output()
    test_dependency_discovery()
    test_missing_dependency_fails_without_output()
    test_optional_smoke()
    print("all liftkit_vf2 tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
