# TEST+DOWN cursor 7->8 and cursor-8 corridor native (v0717)

## Result

Two more `0x9ff8` selector-`0x11` combos run fully native under strict
per-block differential (all MATCH to `0x0a010`/`0x1645c`), advancing
the operator submenu cursor onto the second packed-bit settings row:

| frame | input / prev / released | nav | a5 | path | probe |
| --- | --- | --- | --- | --- | --- |
| TEST+DOWN | `0x0f001004` / `0x0f000000` / `0` | `0x1004` | 7->8 | advance + observed exit | 3282 steps |
| release | `0x0f000000` / `0x0f001004` / `0x1004` | `0` | 8 | match_count render | proven by MATCH |
| idle (stable) | `0x0f000000` / `0x0f000000` / `0` | `0` | 8 | match_count render | proven by MATCH |

Chained proof: the v0716 corridor extended with 7->8 + release + idle
= 27 frames, every frame MATCH (4581 insns on the 7->8 advance frame;
release+idle 9148 insns; per-frame render work varies by row).

## Measured oracle behavior

- **7->8 advance** (from the `ga-dn78` `0x9ff8` failure state): 3282
  steps = 232 prefix + **3050** body — the plain single-step observed
  tail (`0x5a7b8` loop, `stob 0x08` to `0x5000a5`,
  `ret`/`0x10b78`/`0x10b84`/`0xa6f4` chain to `0xa010`),
  call/return delta 43/43. Register poststate: the standard advance
  override set (r14 = frame-counter-minus-one, r15=`0x8a00`, g1/g2/g6
  live, g0=1) with r25 = `0x01000a98` (the row-7 text destination —
  the row being left). The row-7 marker at `0x01000a98` ends `0x8020`.
- **Cursor-8 release/idle**: the predicted 3277-step match_count shape
  (same as rows 1/2/3/7) with r25 = `0x01000b98` (row-8 destination
  from the 0x5b340 row table) MATCHed on first implementation — the
  differential itself proves the poststate pins (registers, counters,
  RAM) equal to the oracle; exit a5=`08` verified in the snapshot.

## Gaps closed

1. **Gate**: `test_nav5_entry` (advance triple at a5=7, 3050/37 delta
   branch) and `test_cursor8_entry` (release + idle triples at a5=8),
   the latter also admitted through the released-flags gate clause.
2. **Finish routing**: the nav5 frame (a5=7, delta=+1) bypasses the
   match_count branch (flag-gated a5=7 arm) **and** the packed-flag
   finish (`test_nav5_entry == 0` guard added — the first advance
   whose source row is a packed-bit row); the cursor-8 stable combos
   route to match_count (a5=8 + flag). Base-combo paths unchanged.
3. **Registers**: nav5 mirrors nav2/nav3 with r25=`0x01000a98`;
   cursor-8 extends the stable override block with r25=`0x01000b98`.
   `0x5ff684` live-g1 spill chain extended (10 levels).
4. **CC**: nav5 via the existing input-keyed NONE exception
   (`0x0f001004`); cursor-8 stable via the historical EQUAL default.

## Still open (next)

- Cursor 8->9 DOWN (single-step expected; stable r25=`0x01000c98`
  pre-measured). Verified fail-closed at `0x9ff8`/`0xa6c0`; the
  `ga-dn89` failure state is captured for the next probe.
- Then 9->10 (string-indexed row), 10->11, 11->12, 12->13, 13->14;
  rows 8-14 destinations pre-measured from the row table.
- UP (`0x2000` family, `--input 0x20001`) and per-row LEFT/RIGHT
  edits (`--input 0x20004`/`0x20008`) on rows 7-14 — new combo
  families (the base mapping already decodes `0x2000` -> -1/3048/37
  but no exact combo admits it yet).
- Attract proper remains device-blocked (v0354/v0366).
