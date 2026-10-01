#!/usr/bin/env python3
"""ROM-independent unit tests for the positive bit-6 sweep driver.

    python3 decomp/i960/tools/test_validate_game_info_positive_bit6_sweep.py
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from validate_game_info_positive_bit6_sweep import (  # noqa: E402
    BIT6_INDEX,
    MASK_COUNT,
    bit6_masks,
    parse_masks,
)


def test_parse_masks():
    assert parse_masks("16,23,24") == [16, 23, 24]
    assert parse_masks(" 0x10 , 0x17 ") == [16, 23]
    assert parse_masks("") == []
    print("ok: parse_masks")


def test_full_sweep_only_keeps_bit6_masks():
    masks = list(bit6_masks(0, 1))
    assert len(masks) == MASK_COUNT // 2
    assert all(mask & (1 << BIT6_INDEX) for mask in masks)
    assert masks == sorted(masks)
    print("ok: full sweep honours bit 6")


def test_shards_partition_without_overlap():
    masks = list(bit6_masks(0, 1))
    for shards in (1, 2, 4, 7):
        union = []
        for shard in range(shards):
            union.extend(bit6_masks(shard, shards))
        assert sorted(union) == masks
    print("ok: shards partition the sweep")


def test_explicit_masks_are_sharded_by_index():
    explicit = [16, 24, 48, 56, 80]
    assert list(bit6_masks(0, 1, explicit)) == explicit
    assert list(bit6_masks(0, 2, explicit)) == [16, 48, 80]
    assert list(bit6_masks(1, 2, explicit)) == [24, 56]
    print("ok: explicit mask list shards by index")


def main() -> int:
    test_parse_masks()
    test_full_sweep_only_keeps_bit6_masks()
    test_shards_partition_without_overlap()
    test_explicit_masks_are_sharded_by_index()
    print("all positive-bit6 sweep tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
