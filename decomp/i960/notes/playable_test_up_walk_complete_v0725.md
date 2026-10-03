# TEST+UP full walk 10->0, 0->15 wrap and EXIT-row corridor native (v0725)

## Result

The UP navigation family completes: every TEST+UP advance from row 14
down to row 0, the 0->15 wrap, and all post-UP release/idle frames run
fully native under strict per-block differential (all MATCH to
`0x0a010`/`0x1645c`):

| frame | input / prev / released | nav | a5 | path | probe |
| --- | --- | --- | --- | --- | --- |
| TEST+UP x7 | `0x0f002004` / `0x0f000000` / `0` | `0x2004` | 10->9, 9->8, 8->7, 3->2, 2->1, 1->0 + 7->3 skip | advance + observed exit | 3279 (skip: 3297) |
| 7->3 UP skip | same | `0x2004` | 7->3 | advance + observed exit | 3297 steps |
| 0->15 wrap | same | `0x2004` | 0->15 | advance + observed exit | 3280 steps |
| post-UP release x9 | `0x0f000000` / `0x0f002004` / `0x2004` | `0` | 9..1, 0, 15 | match_count / exit_control / initialize / special | 3277 (row 0: 3276) |
| idle x9 | `0x0f000000` / `0x0f000000` / `0` | `0` | 9..1, 0, 15 | same finishes | proven by MATCH |

UP-walk proof: 14->13->12->11->10->9->8->7->3->2->1->0 + wrap to 15 —
24 consecutive UP frames all MATCH, plus the per-row release+idle
pairs (9148-9150 insns each).

## Measured oracle behavior

- **UP single-step** (all probed): 3279 steps = 232 + **3047** body,
  43/43 calls, r25 = source-row destination, g0=`0xffffffff` (-1 via
  the generic 0x5ff680 write).
- **7->3 UP skip**: 3297 steps = 232 + **3065** (3047 + 3 reverse
  skip iterations x 6 steps over rows 6/5/4), calls 43/43, stob 0x03,
  r25=`0x01000a98`.
- **0->15 wrap**: 3280 steps = 232 + **3048** (the wrap branch costs
  one step over the single-step path, matching the plain 0x2000
  mapping), calls 43/43, stob 0x0f, r25=`0x01001398`, exit a5=`0f`.
- **Post-UP release at rows 1-14**: UP-flavored latch triple
  (`0x0f000000`/`0x0f002004`/`0x2004`), same finishes as the DOWN
  releases (match_count rows 1/2/3/7/8/9/12/13/14; special-assignment
  rows 10/11).
- **Post-UP release at row 0** (exit_control, 3276 steps = 232 +
  3044 body, calls 38): r25=`0x01001398` (the pin), CC GREATER kept
  intact (existing condition-impl arm covers input `0x0f000000` at
  a5=0), plus the stable overrides (r14=counter-1, r15=0x8a00, live
  g's). Probe-vs-differential lesson: the probe's calls delta (44)
  overcounts by 6 against the block contract — the differential is
  authoritative (38).
- **Post-UP release/idle at row 15 (EXIT)** (initialize, 3276 steps
  = 232 + 3044, calls 38): r25=`0x01001298` descriptor-driven, CC
  EQUAL, same stable overrides.

## Gaps closed

1. **Gates**: `test_up5..up12_entry` (10->9 .. 1->0, 7->3 skip, 0->15
   wrap), `test_up0rel_entry` (row-0 release), `test_cursor15_entry`
   (EXIT-row release/idle), post-UP release arms for cursor 1/2/3/
   7/8/9/12/13 (match_count rows), 10/11 (special-assignment rows).
2. **Routing**: up9 bypasses the a5=3 difficulty branch;
   up10/up11 bypass the unflagged match_count a5=1/2 arms;
   up10 also needed the delta-branch admission (caught by a fail-
   closed 1064-insn stop before the fix); up12 (a5=0, delta=-1)
   bypasses the a5==0 exit_control branch naturally; up0rel/cursor15
   extend the exit_control/initialize branches with the stable
   override blocks. Base-combo paths unchanged.
3. **CC**: all UP advances via the extended input-keyed NONE exception
   (`0x0f002004`); row-0 release keeps GREATER intact; row-15 stable
   via the initialize EQUAL pin.
4. **Spill chain**: all new combos admitted (if/else chain).

## Fail-closed verified

- DOWN at row 15 (EXIT): stops at `0x9ff8`/`0xa6c0` (unmeasured
  wrap/exit-activation shape).
- All 10 regression suites pass.

## Still open (next)

- Per-row LEFT/RIGHT edits on rows 7-14 (`--input 0x20004`/`0x20008`,
  nav `0x100`/`0x200` edit combos) — the operator-settings play
  value; the base mapping decodes edit_delta but no exact combo
  admits the shapes yet.
- DOWN at row 15 (wrap to 0 or EXIT activation via TEST).
- Attract proper remains device-blocked (v0354/v0366).
