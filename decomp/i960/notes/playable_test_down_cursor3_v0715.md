# TEST+DOWN cursor 2->3 and cursor-3 corridor native (v0715)

## Result

Three more `0x9ff8` selector-`0x11` combos run fully native under strict
per-block differential (all MATCH to `0x0a010`/`0x1645c`), extending the
operator submenu walk to stable cursor 3 (the DIFFICULTY row):

| frame | input / prev / released | nav | a5 | path | probe |
| --- | --- | --- | --- | --- | --- |
| TEST+DOWN | `0x0f001004` / `0x0f000000` / `0` | `0x1004` | 2->3 | advance + observed exit | 3282 steps |
| release | `0x0f000000` / `0x0f001004` / `0x1004` | `0` | 3 | match_count render | 3277 |
| idle (stable) | `0x0f000000` / `0x0f000000` / `0` | `0` | 3 | match_count render | 3277 |

Full-chain proof: sixth-fresh + 5 DOWN taps (walk5, 10 frames) + TESTx2
+ DOWN 0->1 + release + idle + DOWN 1->2 + release + idle + DOWN 2->3
+ release + idle = 21 frames, every frame MATCH (5184 insns on the 2->3
advance frame, 5178 on each cursor-3 stable frame).

## Measured oracle behavior

- **2->3 advance** (from the `ga-dn23` `0x9ff8` failure state): the
  same 3282 steps as 0->1/1->2 with the identical observed tail
  (`0x5a7b8` loop, `stob` cursor 2->3 to `0x5000a5`, then
  `ret`/`0x10b78`/`0x10b84`/`0xa6f4` chain to `0xa010`). Call/return
  delta 43/43 like the earlier advances. All 32 registers at `0xa010`
  equal the `finish_observed` base pins plus the nav2-style overrides:
  r14 = frame-counter-minus-one, r15=`0x8a00`, g1/g2/g6 live
  (`0x3f4f5c29`/`0xc0a0a3d7`/`0x55b6`), r25 = `0x01000418`
  (row-2 destination). The row-2 marker word at `0x01000418` ends
  `0x8020` (the transient `0x801c` write is invisible at the block
  boundary); no marker is written at the new row until the next frame.
- **Cursor-3 release** (from the `ga-c3rel` `0x9ff8` failure state):
  3277 steps, match_count tail
  (`0x60b90`/`0x5a9c0`/`0x5aa08` -> `0x10b78`/`0x10b84`/`0xa6f4` ->
  `0xa010`), CC EQUAL/`ac&7=2` — the same stable-render shape as
  cursor 1/2, NOT a difficulty-specific path. Register poststate:
  r9=`0xffffffff`, r14 = frame-counter-minus-one (`0x500020`=25 ->
  r14=0x18), r15=`0x8a00`, g1/g2/g6 live, and the arrow written at
  `0x01000598`.
- **Key structural fact**: row 3's text destination is NOT on the
  `0x01000318+(a5-1)*0x100` row grid that served rows 1/2 — the row-3
  label sits at `0x0100059c` (marker `0x01000598`), so the cursor-3
  stable frames need an explicit `r25=0x01000598` override where the
  selection-based match_count pin served cursor 1/2. The advance
  frames keep the literal row-2 pin (`0x01000418`).

## Gaps closed

1. **Gate**: `test_nav3_entry` (`0x0f001004`/`0x0f000000`/0/`0x1004`/
   a5=2/a6=0xff) + delta mapping alongside `0x1000` (same 3050/37);
   `test_cursor3_entry` (release + idle triples, a5=3), also admitted
   through the released-flags gate clause.
2. **Finish routing**: nav3 skips the `a5 in {1,2}` match_count branch
   (flag-exempted) into the observed exit; the cursor-3 stable combos
   route to match_count (a5=3 + flag) instead of the difficulty
   finish — the base-combo a5=3 difficulty path is unchanged.
3. **Registers**: nav3 mirrors nav2 exactly (counter rule, r15, g-snaps,
   `0x5ff684` live-g1 spill, r25=`0x01000418`); cursor-3 release/idle
   extend the cursor-1/2 override block (r9/r14/r15/g-snaps) plus the
   new r25=`0x01000598` row-grid exception.
4. **CC**: nav3 via the existing input-keyed NONE exception
   (`0x0f001004`); cursor-3 stable frames via the historical EQUAL
   default (measured EQUAL at `0xa010`).

## Still open (next)

- Cursor 3->4 DOWN (`a5=3`, nav `0x1000 or 0x1004`, new latches): the
  failure state is reproducible from `ga-c3-stable` + `--input 0x20002`
  (verified fail-closed at `0x9ff8`/`0xa6c0`). Row 4's destination must
  be measured (the row grid is not uniform); expect observed exit with
  r25 = the row-3 destination `0x01000598`.
- UP (`0x2000` family) at any cursor; per-row edits (LEFT/RIGHT) on the
  value rows; EXIT row.
- The nav `0x1000` shape at a5=1/2 (TEST held + DOWN new) and the
  `0x1004` shape at a5=0/1 for TEST-held chains remain unmeasured
  siblings — fail-closed.
- Attract proper remains device-blocked (v0354/v0366).
