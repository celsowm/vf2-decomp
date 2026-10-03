# TEST+DOWN cursor 3->7 (advance-skip) and cursor-7 corridor native (v0716)

## Result

Three more `0x9ff8` selector-`0x11` combos run fully native under strict
per-block differential (all MATCH to `0x0a010`/`0x1645c`), jumping the
operator submenu cursor across the rows 4/5/6 gap onto the first
packed-bit settings row:

| frame | input / prev / released | nav | a5 | path | probe |
| --- | --- | --- | --- | --- | --- |
| TEST+DOWN | `0x0f001004` / `0x0f000000` / `0` | `0x1004` | 3->7 | advance + observed exit | 3300 steps |
| release | `0x0f000000` / `0x0f001004` / `0x1004` | `0` | 7 | match_count render | 3277 |
| idle (stable) | `0x0f000000` / `0x0f000000` / `0` | `0` | 7 | match_count render | 3277 |

Chained proof: the v0715 corridor (sixth-fresh + walk + TESTx2 +
0->1->2->3) extended with 3->7 + release + idle = 24 frames, every
frame MATCH (5205 insns on the 3->7 advance frame, 5175 on each
cursor-7 stable frame).

## Measured oracle behavior

- **Row table** (0x5b340 descriptors, from the oracle's own render
  reads): rows 4/5/6 carry the `0x200` advance-skip flag, so a DOWN
  advance from row 3 skips three candidates and lands on row 7. All 16
  label destinations are now known: row 7 marker `0x01000a98` (label
  `0x01000a9c`), rows 8-14 at `0x01000b98..0x01001198` (uniform `0x100`
  spacing in that span), row 15 (EXIT) `0x01001298`, row 0 header
  `0x01001398` (the observed base pin).
- **3->7 advance** (from the `ga-dn37` `0x9ff8` failure state): 3300
  steps = 232 prefix + **3068** body — each advance-skip loop
  iteration costs 6 steps (`addi/cmpible/cmpibge/ld/ld/bbs` at
  `0x5a7b8`/`0x5a7dc`, three skips), and the skip loop is inline so
  the call/return delta stays 43/43 like the single-step advances.
  Identical observed tail after the skips (`stob 0x07` to `0x5000a5`,
  `ret`/`0x10b78`/`0x10b84`/`0xa6f4` chain to `0xa010`). Register
  poststate: same override set as nav2/nav3 with r25 = `0x01000598`
  (the row-3 text destination — the row being left); r14 =
  frame-counter-minus-one, r15=`0x8a00`, g1/g2/g6 live, g0=1.
  The row-3 marker at `0x01000598` ends `0x8020`.
- **Cursor-7 release** (from the `ga-c7rel` `0x9ff8` failure state):
  3277 steps, the match_count tail and CC EQUAL like cursor 1/2/3 —
  the stable frames do NOT take the packed-flag path their row type
  would suggest. r25 = `0x01000a98` (row-7 destination), arrow
  `0x801c` written at `0x01000a98`, r9=`0xffffffff`,
  r14=counter-minus-one, r15=`0x8a00`, g's live.

## Gaps closed

1. **Gate**: `test_nav4_entry` (advance triple at a5=3) with its own
   delta branch pinning **3068/37** (skip-adjusted instruction count);
   `test_cursor7_entry` (release + idle triples, a5=7), also admitted
   through the released-flags gate clause.
2. **Finish routing**: the nav4 frame bypasses both the match_count
   branch (flag-gated arms) and the a5=3 difficulty branch
   (`test_nav4_entry == 0` guard added); the cursor-7 stable combos
   route to match_count (a5=7 + flag) instead of the packed-flag
   finish — the base-combo a5=7 packed path is unchanged.
3. **Registers**: nav4 mirrors nav2/nav3 with r25=`0x01000598`;
   cursor-7 release/idle extend the stable override block with
   r25=`0x01000a98`. `0x5ff684` live-g1 spill chain extended.
4. **CC**: nav4 via the existing input-keyed NONE exception
   (`0x0f001004`); cursor-7 stable via the historical EQUAL default.

## Still open (next)

- Cursor 7->8 DOWN (single-step, no skips: expect 3282/3050 shape,
  r25=`0x01000a98` on the advance; stable r25=`0x01000b98`). Verified
  fail-closed at `0x9ff8`/`0xa6c0`. Then 8->9 etc. through row 14;
  rows 8-14 destinations pre-measured from the row table.
- UP (`0x2000` family, `--input 0x20001`): verified fail-closed at
  cursor 7; new direction shape (base mapping 3048/37, delta=-1).
- Per-row edits (LEFT/RIGHT, `--input 0x20004`/`0x20008`) on rows
  7-14 — the operator-settings play value; each row is a new combo
  family (value write + CRC recompute).
- Rows 4/5/6 are never cursor-stable (advance-skipped by construction).
- Attract proper remains device-blocked (v0354/v0366).
