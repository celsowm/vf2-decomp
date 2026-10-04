#!/usr/bin/env python3
"""Unit tests for tools/python/taint.py.

Runs standalone (no pytest required) and never invokes vf2i960, so the
targeted dynamic taint can be validated on synthetic traces without ROM:

    python tools/python/test_taint.py

The tests cover the v0704 contract documented in AGENTS.md next-work #3:

    branch 0x00018698 depends on:
      fighter0 + 0x1a4 bit 6
      fighter0 + 0x5b6
"""

from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from taint import (
    DEFAULT_WINDOW,
    is_branch,
    parse_mem_operands,
    regs_in_ops,
    run_taint,
    tag_for_address,
)


def _write_trace(path: Path, records: list) -> None:
    path.write_text("\n".join(json.dumps(r) for r in records) + "\n",
                    encoding="utf-8")


def test_tag_for_address_in_window():
    bases = {"fighter0": 0x510000, "fighter1": 0x520000}
    # fighter0 + 0x1a4 (note: tag uses 4-hex-digit width)
    assert tag_for_address(0x510000 + 0x1a4, bases, DEFAULT_WINDOW) \
        == "fighter0 + 0x01a4"
    # fighter1 + 0x5b6
    assert tag_for_address(0x520000 + 0x5b6, bases, DEFAULT_WINDOW) \
        == "fighter1 + 0x05b6"
    # Just outside the window
    assert tag_for_address(0x510000 + DEFAULT_WINDOW, bases, DEFAULT_WINDOW) is None
    # Random work-RAM address
    assert tag_for_address(0x00884000, bases, DEFAULT_WINDOW) is None
    print("ok: tag_for_address assigns fighter window correctly")


def test_parse_mem_operands_extracts_offset_base():
    ops = "0x1a4(r10),r6"
    parsed = parse_mem_operands(ops)
    assert parsed == [(0x1a4, "r10")], parsed
    # Plain register operands only
    assert parse_mem_operands("r4,r5") == []
    # Multiple memory operands
    ops2 = "0x10(r3),r4  0x20(r5),r6"
    parsed2 = parse_mem_operands(ops2)
    assert (0x10, "r3") in parsed2 and (0x20, "r5") in parsed2
    print("ok: parse_mem_operands extracts (offset, base) pairs")


def test_regs_in_ops_skips_immediates():
    # Should pick up only register operands, not the offset literal.
    ops = "0x1a4(r10),r6"
    assert regs_in_ops(ops) == ["r10", "r6"], regs_in_ops(ops)
    print("ok: regs_in_ops excludes hex literals")


def test_is_branch_recognises_known_forms():
    assert is_branch("bbc") is True
    assert is_branch("bbs") is True
    assert is_branch("be") is True
    assert is_branch("bne") is True
    assert is_branch("cmpibg") is True
    assert is_branch("mov") is False
    assert is_branch("ld") is False
    assert is_branch("st") is False
    print("ok: is_branch covers known conditional forms")


def test_branch_depends_on_fighter_load():
    """The v0704 contract: branch 0x18698 reads fighter0+0x1a4 bit 6."""
    bases = {"fighter0": 0x510000}
    records = [
        # load fighter0+0x1a4 into r6
        {"type": "memory", "step": 1, "kind": "read",
         "address": 0x510000 + 0x1a4, "size": 4},
        {"type": "step", "step": 1, "ip_before": 0x18690,
         "ip_after": 0x18694, "mnemonic": "ld",
         # disasm_map equivalent: ops for "0x1a4(r10),r6"
         "ops": "0x1a4(r10),r6"},
        # bbc 6, r6, target  (branch at 0x18698)
        {"type": "step", "step": 2, "ip_before": 0x18698,
         "ip_after": 0x1869c, "mnemonic": "bbc",
         "ops": "6,r6,0x186a4"},
        {"type": "final", "status": "ok", "halt_reason": "stop address",
         "ip": 0x186a4},
    ]
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        _write_trace(trace, records)
        # Inject the disassembly map directly so we don't shell out.
        disasm_map = {
            0x18690: {"mnemonic": "ld", "ops": "0x1a4(r10),r6", "raw": "ld"},
            0x18698: {"mnemonic": "bbc", "ops": "6,r6,0x186a4", "raw": "bbc"},
        }
        result = run_taint(trace, Path(tmp), Path("vf2i960"), bases,
                           DEFAULT_WINDOW, disasm_map)
    deps = result["branch_deps"][0x18698]
    assert "fighter0 + 0x01a4 bit 6" in deps, deps
    print("ok: branch taint identifies fighter0+0x1a4 bit 6")


def test_branch_depends_on_compare_chain():
    """Two fighter loads, compare, then conditional branch."""
    bases = {"fighter0": 0x510000}
    records = [
        # load fighter0+0x1a4 into r6
        {"type": "memory", "step": 1, "kind": "read",
         "address": 0x510000 + 0x1a4, "size": 4},
        {"type": "step", "step": 1, "ip_before": 0x18690,
         "ip_after": 0x18694, "mnemonic": "ld",
         "ops": "0x1a4(r10),r6"},
        # compare r6, 0
        {"type": "step", "step": 2, "ip_before": 0x18694,
         "ip_after": 0x18698, "mnemonic": "cmpi",
         "ops": "r6,0"},
        # conditional branch on the result
        {"type": "step", "step": 3, "ip_before": 0x18698,
         "ip_after": 0x186a4, "mnemonic": "be",
         "ops": "0x186a4"},
        {"type": "final", "status": "ok", "halt_reason": "stop address",
         "ip": 0x186a4},
    ]
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        _write_trace(trace, records)
        disasm_map = {
            0x18690: {"mnemonic": "ld", "ops": "0x1a4(r10),r6", "raw": "ld"},
            0x18694: {"mnemonic": "cmpi", "ops": "r6,0", "raw": "cmpi"},
            0x18698: {"mnemonic": "be", "ops": "0x186a4", "raw": "be"},
        }
        result = run_taint(trace, Path(tmp), Path("vf2i960"), bases,
                           DEFAULT_WINDOW, disasm_map)
    deps = result["branch_deps"][0x18698]
    assert "fighter0 + 0x01a4" in deps, deps
    print("ok: compare-taint chain reaches conditional branch")


def test_branch_independent_of_fighter_window():
    """A branch whose comparison uses only an immediate must not pick up tags."""
    bases = {"fighter0": 0x510000}
    records = [
        {"type": "step", "step": 1, "ip_before": 0x18694,
         "ip_after": 0x18698, "mnemonic": "cmpi",
         "ops": "r6,0"},
        {"type": "step", "step": 2, "ip_before": 0x18698,
         "ip_after": 0x186a4, "mnemonic": "be",
         "ops": "0x186a4"},
        {"type": "final", "status": "ok", "halt_reason": "stop address",
         "ip": 0x186a4},
    ]
    with tempfile.TemporaryDirectory() as tmp:
        trace = Path(tmp) / "case.jsonl"
        _write_trace(trace, records)
        disasm_map = {
            0x18694: {"mnemonic": "cmpi", "ops": "r6,0", "raw": "cmpi"},
            0x18698: {"mnemonic": "be", "ops": "0x186a4", "raw": "be"},
        }
        result = run_taint(trace, Path(tmp), Path("vf2i960"), bases,
                           DEFAULT_WINDOW, disasm_map)
    deps = result["branch_deps"][0x18698]
    assert deps == set() or not any("fighter" in d for d in deps), deps
    print("ok: taint-free compare yields branch with no fighter deps")


def main() -> int:
    test_tag_for_address_in_window()
    test_parse_mem_operands_extracts_offset_base()
    test_regs_in_ops_skips_immediates()
    test_is_branch_recognises_known_forms()
    test_branch_depends_on_fighter_load()
    test_branch_depends_on_compare_chain()
    test_branch_independent_of_fighter_window()
    print("all taint tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())