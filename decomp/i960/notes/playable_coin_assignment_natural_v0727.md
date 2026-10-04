# COIN ASSIGNMENT natural entry, parent walk, edits and EXIT native (v0727)

## Result

Selector-17 index 5 (COIN ASSIGNMENT) is now entered **naturally from
the TEST MENU** and operated end-to-end under strict per-block
differential: TEST press on the `?.COIN ASSIGNMENT` row, the
TEST-held first coin frame, release, settled idles, all twelve parent
navigation transitions (rows 0..5 circular, both directions, both
wraps) with per-row releases and idles, the row-1 COIN CHUTE TYPE
value-edit cycle in both directions and both chute modes, and both
EXIT actions. The return path to the TEST MENU and onward navigation
back into GAME ASSIGNMENT also runs fully native.

All proofs are chained strict differentials from
`out/sixth-fresh.vf2snap` (snapshots in `out/`, not committed):

| leg | frames | measured shapes |
| --- | --- | --- |
| TEST MENU -> coin entry | press 14919, held 6324, release 6318, idle 6321/6318 | block 4420 = 232 cluster prefix + 4188/35 body |
| parent walk forward 1->2->3->4->5->0(wrap) | 15 frames | nav body 4194 (4195 wrap), 34 calls, r25 = source-row cursor, g0 = +/-1 |
| parent walk back 0->5(wrap)->4->3->2->1->0 | 18 frames | nav body 4191 (4192 wrap), 34 calls |
| row-1 edit COMMON->INDIVIDUAL | edit 5935, release 5590, idles 5590/5587/5590 | edit body 4405/38; release+idle body 4060/32, g1 = 0 |
| row-1 edit INDIVIDUAL->COMMON | edit 5793, release 5721, idles 5713/5718 | edit body 4266/34; release+idle body 4188/35 (COMMON render) |
| rows 2/3/4 TEST edit frames | 5932/5935/5930 | bodies 4405/4405/4403, render pre-edit values |
| EXIT+ (TEST) | 20583 | body 19056/74, a4 0x85 -> 0x05, TEST MENU restored |
| EXIT- (KICK) | 5719, release 5715, idle 5718 | body 4185/35 redraw, g0 = -1, AC1/GREATER |
| TEST MENU -> GAME ASSIGNMENT re-entry | 1569/1580/14319/4574 | proven index-4 machinery re-enters cleanly |

## Measured oracle behavior

- **Input-latch tuples, not parked clean latches.** The natural walk
  latches TEST/SERVICE flavors (`0x0f0000xx`) instead of the parked
  `0x0ff7f700`. The frame prefix consumes raw edges before the coin
  handler runs, so the handler observes settled latches. Admitted
  combos live in an exact-measured tuple table (input, previous,
  released, nav, source a5, coin mode); everything else stays
  `VF2_ERROR_UNSUPPORTED`.
- **Edit decode.** The TEST edge (nav `0x4`) and the PUNCH edge (nav
  `0x100`) both decode as the +1 edit; the KICK edge (nav `0x200`)
  is the -1 edit. The parked synthetic nav 0x100/0x200 values map to
  PUNCH/KICK host inputs.
- **Deferred value update.** The edit frame renders the *pre-edit*
  values; the new value lands on the release frame, which re-renders
  with the mode-dependent layout (measured 4060/32 body when the
  INDIVIDUAL layout is drawn, 4188/35 when the COMMON layout is
  drawn). The steady INDIVIDUAL idle keeps the 4060/32 shape.
- **INDIVIDUAL render** (measured tile writes): row 5 value
  `INDIVIDUAL`, row 13 label `COIN CHUTE   ` (no `#1`), the whole
  chute #2 section erased (label as 13 space glyphs, rows 24-33
  cols 30-47 cleared with raw 0x0020 tiles).
- **Edit bodies are mode-dependent on row 1**: COMMON->INDIVIDUAL
  4405/38 vs INDIVIDUAL->COMMON 4266/34 (both measured; the KICK -1
  direction is COMMON-scoped in the tuple table until measured).
- **Poststate rule** (natural-latch frames): cluster locals r14/r15
  keep their caller-frame entry values (r14 is the live
  frame-counter-minus-one, 0x12/0x13/..., against the parked pin 1);
  idle/nav/release shapes keep the flat g1/g2/g6 globals, edit
  shapes pin g0 = new 15-byte checksum, g1 = 0, g2 = 15. The
  post-edit TEST release rendering INDIVIDUAL additionally keeps
  g1 = 0. EXIT+ pins its own 19056/74 poststate with caller r14/r15.
- **EXIT semantics (measured why EXIT- does not leave)**: the real
  exit runs only on the +1 edit decode (TEST/PUNCH at row 0 ->
  19056/74, clears bit 7 of a4 and restores the TEST MENU); the -1
  direction (KICK) redraws the parent menu (4185/35, g0 = -1,
  AC1/GREATER) and stays in COIN ASSIGNMENT.
- **Condition poststate**: `set_main_final_cluster_condition` now
  leaves the bridge-pinned CC intact for phase 0x85 on the measured
  natural-latch input combos (was: forced EQUAL), mirroring the
  index-4 exception.

## Validation

- All chained legs above MATCH under strict per-block differential
  (full live-state equality incl. counters, CC, frames and all
  mutable Model 2A regions).
- Strict regression suite: 10/10 passed
  (`phase17_zero|texture_bridge_differential|native_sixth_dispatch|
  native_runtime$|native_runtime_state|native_differential|
  post_boot_input_profiles|texture_status|texture_counter|
  texture_upload`).

## Still open (explicitly fail-closed)

- Rows 2-4 edit *release* frames: post-edit states (credit index
  != 2, derived credits, preset != 0) are rejected by the gate and
  need value-driven digit renders for rows 6-9/11 plus a measured
  gate widening.
- MANUAL SETTING natural entry and the whole nested a7 editor.
- PUNCH/KICK releases at value rows; INDIVIDUAL-mode walks at other
  rows; KICK edit from INDIVIDUAL mode.
