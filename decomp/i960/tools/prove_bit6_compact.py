#!/usr/bin/env python3
"""Prove the compact state-8 bit-6 admission rule is exactly equivalent
to the committed 15-entry high_3_4 mask list + structured predicate
(src/recovered/hybrid.c).

The admission predicate depends only on combined_state8_flags through:
  bit6 present, bit8 present, low bits {1,2,4} zero/nonzero,
  selected high bits {21,26,29,30,31} count/shape, all-five shape,
  and presence of any foreign bit. Exhaustive check: all 2^10 measured
  combos x foreign {0,1} = 2048 cases.
"""

BIT6 = 1 << 6
BIT8 = 1 << 8
LOW_BITS = (1 << 1) | (1 << 2) | (1 << 4)
HIGH_BITS = (1 << 21) | (1 << 26) | (1 << 29) | (1 << 30) | (1 << 31)
MEASURED_BITS = BIT6 | BIT8 | LOW_BITS | HIGH_BITS
HIGH_LIST = [21, 26, 29, 30, 31]

# The 15 committed constants from hybrid.c (verbatim).
COMMITTED_15 = [
    0x24200140, 0x44200140, 0x84200140, 0x60200140, 0xA0200140,
    0xC0200140, 0x64000140, 0xA4000140, 0xC4000140, 0xE0000140,
    0x64200140, 0xA4200140, 0xC4200140, 0xE0200140, 0xE4000140,
]


def expected_triples_quads():
    from itertools import combinations

    out = set()
    for r in (3, 4):
        for combo in combinations(HIGH_LIST, r):
            flags = BIT6 | BIT8
            for bit in combo:
                flags |= 1 << bit
            out.add(flags)
    return out


def current_predicate(combined):
    sel = combined & HIGH_BITS
    hwf = 0 if sel == 0 else sel & (sel - 1)
    one = sel != 0 and hwf == 0
    two = hwf != 0 and (hwf & (hwf - 1)) == 0
    in_15 = combined in COMMITTED_15
    struct = (
        (combined & BIT6) != 0
        and (combined & ~MEASURED_BITS) == 0
        and (
            sel == 0
            or ((one or two) and (combined & BIT8) != 0
                and (combined & LOW_BITS) == 0)
            or (sel == HIGH_BITS and (combined & BIT8) != 0)
        )
    )
    return in_15 or struct


def compact_predicate(combined):
    sel = combined & HIGH_BITS
    hwf = 0 if sel == 0 else sel & (sel - 1)
    hws = 0 if hwf == 0 else hwf & (hwf - 1)
    hwt = 0 if hws == 0 else hws & (hws - 1)
    one = sel != 0 and hwf == 0
    two = hwf != 0 and hws == 0
    three = hws != 0 and hwt == 0
    four = hwt != 0 and (hwt & (hwt - 1)) == 0
    return (
        (combined & BIT6) != 0
        and (combined & ~MEASURED_BITS) == 0
        and (
            sel == 0
            or ((one or two or three or four)
                and (combined & BIT8) != 0
                and (combined & LOW_BITS) == 0)
            or (sel == HIGH_BITS and (combined & BIT8) != 0)
        )
    )


def main():
    assert set(COMMITTED_15) == expected_triples_quads(), (
        "committed list is not exactly all triples+quads: "
        f"extra={set(COMMITTED_15) - expected_triples_quads()} "
        f"missing={expected_triples_quads() - set(COMMITTED_15)}"
    )
    print(f"15-entry list == all C(5,3)+C(5,4) triples/quads with bit6+bit8, no lows")
    checked = 0
    admitted = 0
    for measured in range(1 << 10):
        for foreign in (0, 1):
            combined = 0
            for index, bit in enumerate([1, 2, 4, 8, 6, 21, 26, 29, 30, 31]):
                if measured & (1 << index):
                    combined |= 1 << bit
            if foreign:
                combined |= 1 << 17  # arbitrary unmeasured bit
            old = current_predicate(combined)
            new = compact_predicate(combined)
            if old != new:
                print(f"MISMATCH at 0x{combined:08x}: current={old} compact={new}")
                return 1
            checked += 1
            admitted += bool(new)
    print(f"equivalent on all {checked} domain cases ({admitted} admitted)")


if __name__ == "__main__":
    raise SystemExit(main())
