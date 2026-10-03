# TEST+DOWN cursor 1->2 and cursor-2 corridor native (v0714)

## Result

Three more `0x9ff8` selector-`0x11` combos run fully native under strict
per-block differential (all MATCH to `0x0a010`/`0x1645c`), extending the
operator submenu walk to stable cursor 2:

| frame | input / prev / released | nav | a5 | path | probe |
| --- | --- | --- | --- | --- | --- |
| TEST+DOWN | `0x0f001004` / `0x0f000000` / `0` | `0x1004` | 1->2 | advance + observed exit | 3282 steps |
| release | `0x0f000000` / `0x0f001004` / `0x1004` | `0` | 2 | match_count render | 3277 |
| idle (stable) | `0x0f000000` / `0x0f000000` / `0` | `0` | 2 | match_count render | 3277 |

Chained ct5->ct9 MATCH consecutively (ct5 = cursor-1 idle from the
v0712 corridor, rebuilt: sixth-fresh + walk + TESTx2 + DOWN +
release + idle, every frame MATCH).

## Measured oracle behavior (1->2 frame, `dn12` failure state)

- Same 3282-step shape as the 0->1 DOWN (identical tail: advance loop
  `0x5a7b8` with `stob` cursor 1->2, then `ret`/`0x10b78`/`0x10b84`/
  `0xA6F4` chain to `0xA010`). Exit is the OBSERVED shape, not
  match_count: `a5 in {1,2} + nav!=0` takes the advance+observed exit
  (base `a5 in {1,2} + nav` behavior untouched: still match_count,
  unmeasured either way).
- Nav decode is a MASK, not equality: `0x59014` does
  `and 0x08001008 + cmpo + be`, and the direction helper (`0x60b50`:
  `bbs 13` / mask-test) yields +1 identically for `0x1000`/`0x1004`
  (the `0x4` TEST-edge bit is ignored). Native admits `0x1004` only
  via the exact combo, keeping `== 0x1000` decode elsewhere.

## Gaps closed

1. **Gate**: `test_nav2_entry` (`0x0f001004`/`0x0f000000`/0/`0x1004`/
   a5=1/a6=0xff) + delta mapping alongside `0x1000` (same 3050/37);
   `test_cursor2_entry` (release + idle triples, a5=2).
2. **Finish routing**: nav2 skips the `a5 in {1,2}` match_count branch
   (flag-exempted) into the observed exit; base combos unchanged.
3. **Registers**: nav2 `r25=0x01000318` (row-1 text destination, not the
   observed base pin `0x01001398`); r14 = frame-counter-minus-one
   (automatic); r15=`0x8a00`; g1/g2/g6 live snapshots; `0x5ff684`
   live-g1 spill. Cursor-2 release/idle reuse the cursor-1 override
   block (extended): r9=`0xffffffff`, r15=`0x8a00`, counter rule,
   g-snaps, spill; r25 needs NO override (selection-based base pin
   `0x01000318+(a5-1)*0x100` already gives row 2).
4. **CC**: NONE via the existing input-keyed exception (input
   `0x0f001004`, cursor already overwritten).

## Still open (next)

- Cursor 2->3 DOWN (`a5=2`, nav `0x1000 or 0x1004`, new latches): same
  recipe; expect observed exit + r25 = row-2 destination. Then per-step
  to EXIT (row 15); UP (`0x2000` family); PUNCH/service edits per row.
- Natural TEST MENU EXIT leads back to TEST MENU (v0354 warm-boot
  trajectory, device-gated attract beyond): the play value now is
  operator configuration rows, not EXIT.
- Attract proper remains device-blocked (v0354/v0366: `0x550000`
  video-ready gate at sel3 phase 14, coli spin, nav-gate input
  modeling). No session change alters that assessment.
