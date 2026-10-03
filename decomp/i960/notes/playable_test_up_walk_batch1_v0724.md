# TEST+UP cursor 14->13->12->11->10 corridor native (v0724)

## Result

Eight `0x9ff8` selector-`0x11` combos run fully native under strict
per-block differential (all MATCH to `0x0a010`/`0x1645c`), walking the
operator submenu cursor back UP through rows 14->13->12->11->10:

| frame | input / prev / released | nav | a5 | path | probe |
| --- | --- | --- | --- | --- | --- |
| TEST+UP x4 | `0x0f002004` / `0x0f000000` / `0` | `0x2004` | 14->13, 13->12, 12->11, 11->10 | advance + observed exit | 3279 steps each |
| post-UP release x4 | `0x0f000000` / `0x0f002004` / `0x2004` | `0` | 13, 12, 11, 10 | match_count / special render | 3277 / proven by MATCH |
| idle x4 | `0x0f000000` / `0x0f000000` / `0` | `0` | 13, 12, 11, 10 | existing stable paths | MATCH |

UP-walk proof: from `ga-cE-stable` (cursor 14): UP + release + idle
repeated 4x = 12 frames, every frame MATCH (4578/4581 insns on the UP
advance frames; 9150 on each release+idle pair).

## Measured oracle behavior

- **UP advance shape** (all four probed: `ga-upE`, `ga-upD`, `ga-upC`,
  `ga-upB` failure states): **3279 steps** = 232 prefix + **3047**
  body — 3 steps shorter than the DOWN single-step path — with the
  identical observed tail (`0x5a7b8` loop, stob cursor-1 to
  `0x5000a5`, `ret`/`0x10b78`/`0x10b84`/`0xa6f4` chain to `0xa010`),
  call/return delta 43/43. Register poststate: the standard advance
  override set (r14 = frame-counter-minus-one, r15=`0x8a00`, g1/g2/g6
  live) with r25 = the row-being-left destination and g0=`0xffffffff`
  (the -1 delta via the generic `0x5ff680` write). CC: same cleared
  tail as DOWN (NONE at the bridge boundary).
- **Post-UP release** (probed at row 13: `ga-uprelD`): the release
  latch triple is UP-flavored (input `0x0f000000`, previous
  `0x0f002004`, released `0x2004`) but the run is the same 3277-step
  match_count render/exit (CC EQUAL) with the row destination in r25.
  The settled idle triple is direction-independent (already proven).
- Rows 13/12 render post-UP releases via match_count; rows 11/10 via
  the special-assignment finish — same finish as their DOWN-release
  counterparts (direction affects only the latch triple).

## Gaps closed

1. **Gate**: `test_up1_entry`..`test_up4_entry` (UP advance triple at
   a5=14/13/12/11, shared 3047/37 delta branch for the `0x2004`
   shape); post-UP release arms added to the cursor13/cursor12/
   cursor11/cursor10 gates (idle arms already covered both
   directions). New `test_up_input` (`0x0f002004`) constant.
2. **Finish routing**: UP advances bypass match_count (flag-gated
   arms), the packed-flag finish (guard extended per advance) and
   the special-assignment branch (guard extended for the a5=11
   source row). Base-combo paths unchanged.
3. **Registers**: per-advance r25 = source-row destination
   (`0x01001198`/`0x01000f98`/`0x01000e98` + `0x01000d98` for the
   measured 10->9 probe); standard r14/r15/g-snaps; spill chain
   extended.
4. **CC**: the input-keyed NONE exception extended to `0x0f002004`
   (identical oracle tail; proven by the four UP MATCHes).

## Still open (next)

- Cursor 10->9 UP (measured 3279/3047, r25=`0x01000d98`, exit a5=09;
  failure state + probe captured) then 9->8, 8->7, the 7->3 UP skip
  (rows 4/5/6 skipped in reverse — pin TBD by probe), 3->2, 2->1,
  1->0 and the row-0 UP boundary behavior.
- Per-row LEFT/RIGHT edits on rows 7-14 (new combo families).
- Attract proper remains device-blocked (v0354/v0366).
