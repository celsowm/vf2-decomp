# TEST+DOWN cursor 12->13 and cursor-13 corridor native (v0722)

## Result

Two more `0x9ff8` selector-`0x11` combos run fully native under strict
per-block differential (all MATCH to `0x0a010`/`0x1645c`):

| frame | input / prev / released | nav | a5 | path | probe |
| --- | --- | --- | --- | --- | --- |
| TEST+DOWN | `0x0f001004` / `0x0f000000` / `0` | `0x1004` | 12->13 | advance + observed exit | 3282 steps |
| release | `0x0f000000` / `0x0f001004` / `0x1004` | `0` | 13 | match_count render | proven by MATCH |
| idle (stable) | `0x0f000000` / `0x0f000000` / `0` | `0` | 13 | match_count render | proven by MATCH |

Chained proof: the v0721 corridor extended with 12->13 + release +
idle = 42 frames, every frame MATCH (4584 insns on the advance;
release+idle 9150 insns; exit a5=`0d` verified in snapshots).

## Measured oracle behavior

- **12->13 advance** (from the `ga-dnCD` `0x9ff8` failure state): 3282
  steps = 232 prefix + **3050** body — the plain single-step observed
  tail (`0x5a7b8` loop, `stob 0x0d` to `0x5000a5`,
  `ret`/`0x10b78`/`0x10b84`/`0xa6f4` chain to `0xa010`),
  call/return delta 43/43, r25 = `0x01000f98` (row-12 destination).
  The row-12 marker at `0x01000f98` ends `0x8020`.
- **Cursor-13 release/idle**: the predicted match_count shape (row 13
  is packed bit 4; rows 7/8/9/12 already proved the pattern)
  MATCHed on first implementation with r25 = `0x01001098`.

## Gaps closed

1. **Gate**: `test_nav10_entry` (advance triple at a5=12, 3050/37
   delta branch) and `test_cursor13_entry` (release + idle triples
   at a5=13), the latter also admitted through the released-flags
   gate clause.
2. **Finish routing**: nav10 bypasses the packed-flag finish
   (`test_nav10_entry == 0` guard — a5=12 is a packed-bit source
   row); cursor-13 stable routes to match_count (a5=13 + flag),
   ahead of the packed-flag finish. Base-combo paths unchanged.
3. **Registers**: nav10 with r25=`0x01000f98`; cursor-13 stable with
   r25=`0x01001098`. Spill chain extended.
4. **CC**: nav10 via the existing input-keyed NONE exception;
   cursor-13 stable via the historical EQUAL default.

## Still open (next)

- Cursor 13->14 DOWN — row 14 (packed bit 6) is the LAST settings
  row; stable r25=`0x01001198` pre-measured. Verified fail-closed at
  `0x9ff8`/`0xa6c0`; the `ga-dnDE` failure state is captured.
- Row 15 is EXIT (flags 0x400, no value) — 14->15 advance shape TBD
  by probe (may route to the initialize finish).
- UP (`0x2000` family) and per-row LEFT/RIGHT edits on rows 7-14.
- Attract proper remains device-blocked (v0354/v0366).
