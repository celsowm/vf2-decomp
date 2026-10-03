# TEST+DOWN cursor 11->12 and cursor-12 corridor native (v0721)

## Result

Two more `0x9ff8` selector-`0x11` combos run fully native under strict
per-block differential (all MATCH to `0x0a010`/`0x1645c`):

| frame | input / prev / released | nav | a5 | path | probe |
| --- | --- | --- | --- | --- | --- |
| TEST+DOWN | `0x0f001004` / `0x0f000000` / `0` | `0x1004` | 11->12 | advance + observed exit | 3282 steps |
| release | `0x0f000000` / `0x0f001004` / `0x1004` | `0` | 12 | match_count render | proven by MATCH |
| idle (stable) | `0x0f000000` / `0x0f000000` / `0` | `0` | 12 | match_count render | proven by MATCH |

Chained proof: the v0720 corridor extended with 11->12 + release +
idle = 39 frames, every frame MATCH (4581 insns on the advance;
release+idle 9150 insns; exit a5=`0c` verified in snapshots).

## Measured oracle behavior

- **11->12 advance** (from the `ga-dnBC` `0x9ff8` failure state): 3282
  steps = 232 prefix + **3050** body — the plain single-step observed
  tail (`0x5a7b8` loop, `stob 0x0c` to `0x5000a5`,
  `ret`/`0x10b78`/`0x10b84`/`0xa6f4` chain to `0xa010`),
  call/return delta 43/43, r25 = `0x01000e98` (row-11 destination).
  The row-11 marker at `0x01000e98` ends `0x8020`.
- **Cursor-12 release/idle**: the predicted match_count shape (row 12
  is packed bit 5, but rows 7/8/9 already proved packed-bit rows
  render their stable frames via match_count) MATCHed on first
  implementation with r25 = `0x01000f98`.

## Gaps closed

1. **Gate**: `test_nav9_entry` (advance triple at a5=11, 3050/37 delta
   branch) and `test_cursor12_entry` (release + idle triples at
   a5=12), the latter also admitted through the released-flags gate
   clause.
2. **Finish routing**: nav9 bypasses the special-assignment branch
   (`test_nav9_entry == 0` guard added — a5=11 is the second source
   row routing through special-assignment); cursor-12 stable routes
   to match_count (a5=12 + flag), ahead of the packed-flag finish.
   Base-combo paths unchanged.
3. **Registers**: nav9 with r25=`0x01000e98`; cursor-12 stable with
   r25=`0x01000f98`. Spill chain extended (if/else — no paren risk).
4. **CC**: nav9 via the existing input-keyed NONE exception;
   cursor-12 stable via the historical EQUAL default.

## Still open (next)

- Cursor 12->13 DOWN (row 13 packed bit 4; stable r25=`0x01001098`
  pre-measured). Verified fail-closed at `0x9ff8`/`0xa6c0`; the
  `ga-dnCD` failure state is captured.
- Then 13->14; row 14 (packed bit 6) is the last settings row, row 15
  is EXIT (flags 0x400, no value).
- UP (`0x2000` family) and per-row LEFT/RIGHT edits on rows 7-14.
- Attract proper remains device-blocked (v0354/v0366).
