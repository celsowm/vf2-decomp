# TEST+DOWN cursor 13->14 and cursor-14 corridor native (v0723)

## Result

Two more `0x9ff8` selector-`0x11` combos run fully native under strict
per-block differential (all MATCH to `0x0a010`/`0x1645c`), completing
the walk over all eight settings rows:

| frame | input / prev / released | nav | a5 | path | probe |
| --- | --- | --- | --- | --- | --- |
| TEST+DOWN | `0x0f001004` / `0x0f000000` / `0` | `0x1004` | 13->14 | advance + observed exit | 3282 steps |
| release | `0x0f000000` / `0x0f001004` / `0x1004` | `0` | 14 | match_count render | proven by MATCH |
| idle (stable) | `0x0f000000` / `0x0f000000` / `0` | `0` | 14 | match_count render | proven by MATCH |

Chained proof: the v0722 corridor extended with 13->14 + release +
idle = 45 frames, every frame MATCH (4581 insns on the advance;
release+idle 9150 insns; exit a5=`0e` verified in snapshots).

## Measured oracle behavior

- **13->14 advance** (from the `ga-dnDE` `0x9ff8` failure state): 3282
  steps = 232 prefix + **3050** body — the plain single-step observed
  tail (`0x5a7b8` loop, `stob 0x0e` to `0x5000a5`,
  `ret`/`0x10b78`/`0x10b84`/`0xa6f4` chain to `0xa010`),
  call/return delta 43/43, r25 = `0x01001098` (row-13 destination).
  The row-13 marker at `0x01001098` ends `0x8020`.
- **Cursor-14 release/idle**: the predicted match_count shape (row 14
  is packed bit 6, the last settings row) MATCHed on first
  implementation with r25 = `0x01001198`.

## Gaps closed

1. **Gate**: `test_nav11_entry` (advance triple at a5=13, 3050/37
   delta branch) and `test_cursor14_entry` (release + idle triples
   at a5=14), the latter also admitted through the released-flags
   gate clause.
2. **Finish routing**: nav11 bypasses the packed-flag finish
   (`test_nav11_entry == 0` guard — a5=13 is a packed-bit source
   row); cursor-14 stable routes to match_count (a5=14 + flag),
   ahead of the packed-flag finish. Base-combo paths unchanged.
3. **Registers**: nav11 with r25=`0x01001098`; cursor-14 stable with
   r25=`0x01001198`. Spill chain extended.
4. **CC**: nav11 via the existing input-keyed NONE exception;
   cursor-14 stable via the historical EQUAL default.

## Walk status: all settings rows stable

Every submenu row now has a proven stable corridor: 0 (entry), 1, 2,
3, 7, 8, 9, 10 (special-assignment), 11 (special-assignment), 12, 13,
14. Rows 4/5/6 are advance-skipped by construction (never stable).
Row 15 (EXIT, flags 0x400) is the only unvisited row.

## Still open (next)

- Cursor 14->15 DOWN into EXIT — shape TBD by probe (row 15 has no
  value; may route to the initialize finish). Verified fail-closed at
  `0x9ff8`/`0xa6c0`; the `ga-dnEF` failure state is captured.
- UP (`0x2000` family) and per-row LEFT/RIGHT edits on rows 7-14 —
  the operator-settings play value.
- Attract proper remains device-blocked (v0354/v0366).
