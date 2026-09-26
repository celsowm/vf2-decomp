#!/usr/bin/env python3
"""Standalone tests for tools/python/model2recomp_hints.py."""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from model2recomp_hints import (  # noqa: E402
    Model2RecompHintsError,
    collect_hints,
    resolve_model2recomp_root,
)


def _fake_checkout(root: Path) -> None:
    tools = root / "tools"
    tools.mkdir(parents=True)
    (tools / "i960_lifter.py").write_text(
        """
def reinit_entries(data, max_size):
    return {0x40}

def interrupt_handlers(data, max_size, data_rom=None):
    return {0x80}

def discover_functions(data, max_size, data_rom=None, hints=None):
    return {0x40, 0xC0, *(hints or ())}
""",
        encoding="utf-8",
    )


def test_dependency_discovery_and_no_fallback() -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary) / "model2recomp"
        _fake_checkout(root)
        assert resolve_model2recomp_root(root) == root.resolve()
        assert (
            resolve_model2recomp_root(
                environ={"MODEL2RECOMP_ROOT": str(root)}
            )
            == root.resolve()
        )
        try:
            resolve_model2recomp_root(environ={})
        except Model2RecompHintsError:
            pass
        else:
            raise AssertionError("missing dependency was silently accepted")
    print("ok: discover model2recomp and fail without fallback")


def test_deterministic_sources_and_entries() -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary) / "model2recomp"
        _fake_checkout(root)
        program = Path(temporary) / "program.bin"
        program.write_bytes(bytes(0x100))
        hints = Path(temporary) / "hints.txt"
        hints.write_text("# measured\nentry 0xfc\n", encoding="utf-8")

        report = collect_hints(
            program_bin=program,
            model2recomp_root=root,
            hints_file=hints,
        )
        assert report["sources"]["reinit_iac"] == ["0x00000040"]
        assert report["sources"]["interrupt_table"] == ["0x00000080"]
        assert report["sources"]["runtime_hints"] == ["0x000000fc"]
        assert [entry["address"] for entry in report["entries"]] == [
            "0x00000040",
            "0x00000080",
            "0x000000c0",
            "0x000000fc",
        ]
        assert report["entries"][0]["sources"] == [
            "reinit_iac",
            "static_discovery",
        ]
    print("ok: deterministic entry sources")


def test_malformed_hint_fails() -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary) / "model2recomp"
        _fake_checkout(root)
        program = Path(temporary) / "program.bin"
        program.write_bytes(bytes(16))
        hints = Path(temporary) / "hints.txt"
        hints.write_text("target 0x10\n", encoding="utf-8")
        try:
            collect_hints(
                program_bin=program,
                model2recomp_root=root,
                hints_file=hints,
            )
        except Model2RecompHintsError:
            pass
        else:
            raise AssertionError("malformed hint was accepted")
    print("ok: reject malformed hint")


def main() -> int:
    test_dependency_discovery_and_no_fallback()
    test_deterministic_sources_and_entries()
    test_malformed_hint_fails()
    print("all model2recomp_hints tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
