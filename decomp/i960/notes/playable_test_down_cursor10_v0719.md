# TEST+DOWN cursor 9->10 and cursor-10 corridor native (v0719)

## Result

Two more `0x9ff8` selector-`0x11` combos run fully native under strict
per-block differential (all MATCH to `0x0a010`/`0x1645c`), advancing
the operator submenu cursor onto the string-indexed settings row:

| frame | input / prev / released | nav | a5 | path | probe |
| --- | --- | --- | --- | --- | --- |
| TEST+DOWN | `0x0f001004` / `0x0f000000` / `0` | `0x1004` | 9->10 | advance + observed exit | 3282 steps |
| release | `0x0f000000` / `0x0f001004` / `0x1004` | `0` | 10 | special-assignment render | 3277 |
| idle (stable) | `0x0f000000` / `0x0f000000` / `0` | `0` | 10 | special-assignment render | 3277 shape |

Chained proof: the v0718 corridor extended with 9->10 + release + idle
= 33 frames, every frame MATCH (4581 insns on the 9->10 advance frame;
release+idle 9150 insns; exit a5=`0a` verified in snapshots).

## Measured oracle behavior

- **9->10 advance** (from the `ga-dn9A` `0x9ff8` failure state): 3282
  steps = 232 prefix + **3050** body — the plain single-step observed
  tail (`0x5a7b8` loop, `stob 0x0a` to `0x5000a5`,
  `ret`/`0x10b78`/`0x10b84`/`0xa6f4` chain to `0xa010`),
  call/return delta 43/43, r25 = `0x01000c98` (row-9 destination).
  The row-9 marker at `0x01000c98` ends `0x8020`.
- **Cursor-10 release** (from the `ga-cArel` `0x9ff8` failure state):
  **3277 steps but NOT match_count** — the oracle renders the
  string-indexed row through the special-assignment finish
  (`0x5b010` region before the shared `0x10b78`/`0x10b84`/`0xa6f4`
  tail), writing the row value glyphs (`COUNTRY...` at
  `0x01000d9c`-`0x01000da8` and a second field at `0x01000dd0`-
  `0x01000dda`) plus the arrow `0x801c` at the row-10 marker
  `0x01000d98`. Calls delta 44; r25 = `0x01000d98` (the descriptor-4
  base pin — already correct); r9=`0xffffffff`, r14=counter-minus-one,
  r15=`0x8a00`, g1/g2/g6 live, g0=0, CC EQUAL. The oracle also spills
  live g1 (`0x3f4f5c29`) to `0x5ff684` on this path.

## Gaps closed

1. **Gate**: `test_nav7_entry` (advance triple at a5=9, 3050/37 delta
   branch) and `test_cursor10_entry` (release + idle triples at
   a5=10), the latter also admitted through the released-flags gate
   clause.
2. **Finish routing**: nav7 bypasses match_count (flag-gated a5=9 arm)
   and the packed-flag finish (`test_nav7_entry == 0` guard); the
   cursor-10 stable combos route to the **special-assignment**
   finish (a5=10 arm, previously base-combo only) — the initial
   match_count prediction for this row was wrong and the
   differential caught it before any commit. Base-combo paths
   unchanged.
3. **Registers**: nav7 with r25=`0x01000c98`; cursor-10 stable keeps
   the descriptor-driven r25 and applies the standard stable
   overrides (r9/r14/r15/g-snaps). `0x5ff684` live-g1 spill chain
   extended (14 levels).
4. **CC**: nav7 via the existing input-keyed NONE exception;
   cursor-10 stable via the special-assignment EQUAL pin.

## Still open (next)

- Cursor 10->11 DOWN — row 11 is packed-bit (bit 2) but the a5=10
  source row routes through special-assignment; the advance shape is
  shared but must be probed. Verified fail-closed at
  `0x9ff8`/`0xa6c0`; the `ga-dnAB` failure state is captured.
- Then 11->12, 12->13, 13->14; rows 11-14 destinations pre-measured.
- UP (`0x2000` family) and per-row LEFT/RIGHT edits on rows 7-14.
- Attract proper remains device-blocked (v0354/v0366).
