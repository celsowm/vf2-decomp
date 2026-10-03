# TEST+DOWN cursor 8->9 and cursor-9 corridor native (v0718)

## Result

Two more `0x9ff8` selector-`0x11` combos run fully native under strict
per-block differential (all MATCH to `0x0a010`/`0x1645c`), advancing
the operator submenu cursor onto the third packed-bit settings row:

| frame | input / prev / released | nav | a5 | path | probe |
| --- | --- | --- | --- | --- | --- |
| TEST+DOWN | `0x0f001004` / `0x0f000000` / `0` | `0x1004` | 8->9 | advance + observed exit | 3282 steps |
| release | `0x0f000000` / `0x0f001004` / `0x1004` | `0` | 9 | match_count render | proven by MATCH |
| idle (stable) | `0x0f000000` / `0x0f000000` / `0` | `0` | 9 | match_count render | proven by MATCH |

Chained proof: the v0717 corridor extended with 8->9 + release + idle
= 30 frames, every frame MATCH (4584 insns on the 8->9 advance frame;
release+idle 9150 insns; exit a5=`09` verified in snapshots).

## Measured oracle behavior

- **8->9 advance** (from the `ga-dn89` `0x9ff8` failure state): 3282
  steps = 232 prefix + **3050** body — the plain single-step observed
  tail (`0x5a7b8` loop, `stob 0x09` to `0x5000a5`,
  `ret`/`0x10b78`/`0x10b84`/`0xa6f4` chain to `0xa010`),
  call/return delta 43/43. Register poststate: the standard advance
  override set with r25 = `0x01000b98` (the row-8 text destination).
  The row-8 marker at `0x01000b98` ends `0x8020`.
- **Cursor-9 release/idle**: the predicted 3277-step match_count shape
  with r25 = `0x01000c98` (row-9 destination from the 0x5b340 row
  table) MATCHed on first implementation — the differential proves
  poststate equality with the oracle.

## Gaps closed

1. **Gate**: `test_nav6_entry` (advance triple at a5=8, 3050/37 delta
   branch) and `test_cursor9_entry` (release + idle triples at a5=9),
   the latter also admitted through the released-flags gate clause.
2. **Finish routing**: nav6 bypasses match_count (flag-gated a5=8 arm)
   and the packed-flag finish (`test_nav6_entry == 0` guard added);
   cursor-9 stable routes to match_count (a5=9 + flag).
   Base-combo paths unchanged.
3. **Registers**: nav6 with r25=`0x01000b98`; cursor-9 stable with
   r25=`0x01000c98`. `0x5ff684` live-g1 spill chain extended
   (12 levels).
4. **CC**: nav6 via the existing input-keyed NONE exception;
   cursor-9 stable via the historical EQUAL default.

## Still open (next)

- Cursor 9->10 DOWN — row 10 is the string-indexed row (flags 0x100,
  dest `0x01000d98`); the advance loop shape is shared but the stable
  render may differ. Verified fail-closed at `0x9ff8`/`0xa6c0`; the
  `ga-dn9A` failure state is captured for the next probe.
- Then 10->11, 11->12, 12->13, 13->14; rows 10-14 destinations
  pre-measured from the row table.
- UP (`0x2000` family) and per-row LEFT/RIGHT edits on rows 7-14.
- Attract proper remains device-blocked (v0354/v0366).
