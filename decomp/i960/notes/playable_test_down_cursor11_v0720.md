# TEST+DOWN cursor 10->11 and cursor-11 corridor native (v0720)

## Result

Two more `0x9ff8` selector-`0x11` combos run fully native under strict
per-block differential (all MATCH to `0x0a010`/`0x1645c`):

| frame | input / prev / released | nav | a5 | path | probe |
| --- | --- | --- | --- | --- | --- |
| TEST+DOWN | `0x0f001004` / `0x0f000000` / `0` | `0x1004` | 10->11 | advance + observed exit | 3282 steps |
| release | `0x0f000000` / `0x0f001004` / `0x1004` | `0` | 11 | special-assignment render | proven by MATCH |
| idle (stable) | `0x0f000000` / `0x0f000000` / `0` | `0` | 11 | special-assignment render | proven by MATCH |

Chained proof: the v0719 corridor extended with 10->11 + release +
idle = 36 frames, every frame MATCH (4584 insns on the advance;
release+idle 9150 insns; exit a5=`0b` verified in snapshots).

## Measured oracle behavior

- **10->11 advance** (from the `ga-dnAB` `0x9ff8` failure state): 3282
  steps = 232 prefix + **3050** body — the plain single-step observed
  tail (`0x5a7b8` loop, `stob 0x0b` to `0x5000a5`,
  `ret`/`0x10b78`/`0x10b84`/`0xa6f4` chain to `0xa010`),
  call/return delta 43/43, r25 = `0x01000d98` (row-10 destination).
  The row-10 marker at `0x01000d98` ends `0x8020`.
- **Cursor-11 release/idle**: the predicted special-assignment shape
  (row 11 shares the a5==10/11 finish; packed_bit is -1 for a5=11)
  MATCHed on first implementation with descriptor-driven
  r25 = `0x01000e98` and the standard stable overrides.

## Gaps closed

1. **Gate**: `test_nav8_entry` (advance triple at a5=10, 3050/37 delta
   branch) and `test_cursor11_entry` (release + idle triples at
   a5=11), the latter also admitted through the released-flags gate
   clause.
2. **Finish routing**: nav8 bypasses the special-assignment branch
   (`test_nav8_entry == 0` guard — the first advance whose source row
   routes through special-assignment); cursor-11 stable joins the
   cursor-10 special-assignment override block. Base-combo paths
   unchanged.
3. **Registers**: nav8 with r25=`0x01000d98`; cursor-11 stable keeps
   descriptor-driven r25 with the standard overrides. Spill chain
   extended.
4. **Refactor**: the `0x5ff684` live-g1 spill nested ternary (16
   levels, third paren-count build break) is now an if/else chain
   over the same mutually-exclusive gates — identical priority
   order, proven by the full differential battery (36-frame chain +
   10 regression suites all MATCH/pass).
5. **CC**: nav8 via the existing input-keyed NONE exception;
   cursor-11 stable via the special-assignment EQUAL pin.

## Still open (next)

- Cursor 11->12 DOWN (row 12 packed bit 5; stable r25=`0x01000f98`
  pre-measured). Verified fail-closed at `0x9ff8`/`0xa6c0`; the
  `ga-dnBC` failure state is captured.
- Then 12->13, 13->14; rows 12-14 destinations pre-measured.
- UP (`0x2000` family) and per-row LEFT/RIGHT edits on rows 7-14.
- Attract proper remains device-blocked (v0354/v0366).
