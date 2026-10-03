# TEST+DOWN submenu navigation native (cursor 0->1, release, idle) (v0712)

## Result

Three more `0x9ff8` selector-`0x11` combos run fully native under strict
per-block differential (all MATCH through the second scheduler to
`0x0a010`/`0x1645c`), extending the operator submenu walk to cursor 1:

| frame | input / prev / released | nav | a5 | path | steps |
| --- | --- | --- | --- | --- | --- |
| TEST+DOWN | `0x0f001004` / `0x0f000004` / `0` | `0x1000` (+1) | 0->1 | nav advance + observed exit | 4952 insns |
| release | `0x0f000000` / `0x0f001004` / `0x1004` | `0` | 1 | match_count render | 3277 |
| idle (stable) | `0x0f000000` / `0x0f000000` / `0` | `0` | 1 | match_count render | 3277 |

Release/idle/idle-idle chain MATCHes consecutively (t3->t4->t5->t6);
the submenu sits stably on cursor 1 with live values rendered.

## Measured oracle behavior (TEST+DOWN frame, `dn` failure state)

- Worker reads `0x50002a` (selector), `0x5000a4/5/6` (phase/cursor),
  `0x500700` (input latch), `0x500704` x2 (nav), `0x500804/08`
  (fighter words), `0x508000` x3; writes cursor `0x5000a5` 0->1 via
  the `0x5a7b8` advance loop (`stob r13`), selector mirror
  `0x50002b=0x11` / `0x50002c=0x20000`, full tile re-render.
- Release/idle frames: previous/released latches are UNREAD (no
  `0x500708`/`0x50070c` accesses); only input/nav/phase consumed.
  Writes are selector mirror + full menu render, no cursor write.

## Gaps closed (all in existing scaffolding)

1. **Gate**: `test_nav_entry` (`0x0f001004`/`0x0f000004`/0/`0x1000`/
   a5=0/a6=0xff) and `test_cursor1_entry` (release triple + idle
   triple, a5=1/a6=0xff; released-gate refusal exempted for it).
2. **CC**: DOWN frame ends NONE (tail `cmpibge/stob/ret` through the
   scheduler return clears condition state; the dispatch overwrites
   `0x5000a5` before the condition hook runs, so the exception keys
   on the input combo, not the post-advance cursor). Release/idle
   frames keep the shared EQUAL (match_count finish + existing
   leave-intact for `0x0f000000`).
3. **Registers**: DOWN `r15=0x8a00` (vs base `0x8800`); cursor-1
   `r9=0xffffffff`, `r15=0x8a00`; `g1/g2/g6` keep live entry values
   (gate snapshots, as on the nav=0 TEST paths); `0x5ff684` spills
   live g1 on all three.
4. **r14 = pre-increment frame counter** (new rule, 4 instances):
   the dispatch propagates the stale gate `g8` to `r14`, but the
   native prefix never loads `g8` (stale `0x512980`), so snapshots
   cannot see it. RAM `0x500020` is a per-frame counter bumped in
   the prefix *after* the oracle loads `g8`: gate snapshots show
   `RAM=g8+1` (`0x13/0x12`, `0x14/0x13`, `0x15/0x14`). Native reads
   `0x500020` (which it models: work-ram equality proves the bump)
   and pins `r14 = RAM - 1`. Identical IPs/reads across the three
   frames (diffed exactly); entry regs differ only in `g8`/`r0/r2`/
   `r4/r6`; renders pixel-identical (48/48 rows). Fail-closed on
   counter==0. The rule self-adapts per frame (no pin treadmill);
   any order change fails loudly in cycles.

## Still open (next)

- Cursor 1->2 DOWN (`a5=1`, nav `0x1000`, new latches): same recipe,
  but the nav advance runs from cursor 1 (skip-loop may iterate) and
  exits via the match_count finish (not observed). Then 2->3 ... 15
  (EXIT row) — one measured combo per cursor step.
- UP (`nav=0x2000`), PUNCH/service edits, EXIT selection — unmeasured.
- The `0x500020`-counter/`g8` producer could be modeled in the native
  prefix instead (load-then-bump order); the RAM-minus-one read is
  equivalent and local. If the prefix order ever changes, cycles
  catches it.
