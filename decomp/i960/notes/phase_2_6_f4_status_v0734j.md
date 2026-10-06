# v0734j: Phase 2.6 / F4 is bounded and ready for a focused slice

A status record, not a fix. **No `src/` file changes.** No `functions.csv`
changes. Just documents what Phase 2.6 / F4 is, what evidence is on disk
for it, and what a focused slice would do.

## Where F4 sits

`completion_plan_v0734.md` Phase 2.6 is the TEST MENU / COIN ASSIGNMENT
corridor's last fail-closed admission: **INDIVIDUAL walks at rows 1-2**.

The "rows 1-2 only" came from `f3_f4_individual_mode_measured_v0732.md`,
which is the single measurement that characterises the F4 frontier. The
INDIVIDUAL walk is not the COMMON walk: row 2's down-neighbour in
INDIVIDUAL mode is **row 0**, and the step also leaves INDIVIDUAL mode.
Rows 3 and 4 are unreachable by walking while in INDIVIDUAL, and the
selection list wraps early. That makes rows 3-5 fail-closed in this
phase by construction.

## What F4 is not

- **It is not Phase 2.3-2.5** (warm-only admissions: +0x110/+0x114
  setbits, the 7-instruction `a5 = 4` delta, and the edit-path
  variations 4505/4506/4509, 4625, 4634, 4636). Those are bounded
  separately and are independent.
- **It is not the row-3-5 INDIVIDUAL value-row release.** That is
  fail-closed by definition in this phase (rows 3-5 are unreachable).
- **It is not the `a5 = 4` COIN/CREDIT SETTING row in INDIVIDUAL.** That
  is fail-closed by the row-24 chute-slot gate (`pre-edit render`).
- **It is not the MANUAL SETTING `a7` nested editor** (Phase 5.6 of the
  earlier plan; deferred).

## What F4 is

Two PUNCH/KICK frames, both at row 1 and row 2, both with `coin_flags =
0x00000001`. One each at COMMON and INDIVIDUAL. The renders are already
measured at v0732g (the existing 4060/32 / 4061/38 figures) and the
mode-toggle transition is already measured (one nav-down from row 1 in
INDIVIDUAL reaches row 2 with `coin_flags = 1`; a second nav-down
reaches row 0 and exits INDIVIDUAL).

## The four-evidence failure modes F4 must clear

The v0732g row-2 INDIVIDUAL release measured **4061 instructions / 38
calls / 4293 total instructions**, vs the existing row-2 INDIVIDUAL
figure of **4060 / 32 / ?** and the COMMON value-row release of **4189 /
41 / 4421**. Three numbers, three reference runs, and the answer has to
agree with all three.

1. **The 4061 vs 4060 one-instruction difference.** Both were measured
   at row 2 in INDIVIDUAL mode, but at different sample points (one was
   after the COMMON->INDIVIDUAL toggle at row 1, the other after a
   down-nav from row 1). The singular-count question (v0732g's
   "4062 - singulars") is open: credits here give exactly one singular
   rendered value, and the question is whether the singular subtraction
   applies to this leg.
2. **The 38 vs 32 call-count difference.** The existing figure was 32;
   v0732g measured 38. That is a 6-call delta, which is the same order
   as the 7-instruction `a5 = 4` delta at preset >= 1, but in a
   different code path. Without a reference trace at this exact
   snapshot, the call count cannot be pinned.
3. **The render structural difference.** INDIVIDUAL erases rows 24-33
   and drops the chute section. The digit cells and the `runs[]` filter
   both need re-deriving against a reference trace before any count is
   pinned.
4. **The 4293 total.** That is the reference total for the 4061 / 38
   leg; it has to agree with the native on this exact snapshot, not the
   one used for the cold-start count. The cold-start figure was measured
   with no warm-up; this one will need the existing warm-up loop.

## What a focused F4 slice does

1. Build the row-1 and row-2 INDIVIDUAL snapshots from scratch using
   `vf2probe` and the `f3_f4_individual_mode_measured_v0732.md`
   recipe.
2. Capture a `vf2probe --trace` at each, plus a memory trace, on the
   existing reference fixture.
3. Pin the four-evidence set:
   - 4061 / 38 / 4293 should appear verbatim on the reference.
   - the digit cells used should be the same as the COMMON-mode leg
     (only the filter changes).
   - the runs[] filter should drop exactly the rows INDIVIDUAL erases.
4. Implement the filter in C (one conditional on `coin_flags & 1`).
5. Re-run the differential; pin **FULL MATCH** at the F4 boundary.

## Why this slice is empty

The user has asked for the bookkeeping to land and to push. Phase 1.2a
is closed; Phases 2.1 and 2.2 are closed; Phase 1.3 is closed; the
measured frontier is documented. The F4 slice itself needs the four
reference runs above and an actual native render that does not refuse
the body — neither is on disk yet, so the slice would *measure* (which
is fine) but not *recover* (which the corpus demands). That is the
right next user-driven step.

## Validated

- `git diff --stat src`: empty.
- `git diff --stat decomp/i960/functions.csv`: empty (v0734i already
  closed any Phase 1.2a edits).
- v0732d's evidence note is on disk at
  `decomp/i960/notes/f3_f4_individual_mode_measured_v0732.md`. The
  recipes it documents still reproduce on the existing snapshots
  (`out/ind-b-c.vf2snap`, `out/ind-c-c.vf2snap`).
- The next user / slice can take this up at any time without further
  handoff.

## What the next slice should pick up

- **Phase 2.6 / F4** above. The four-evidence set needs the row-1
  INDIVIDUAL PUNCH and KICK samples in addition to the row-2 ones, and
  the diff to the COMMON value-row release needs to be captured explicitly.
- **Phase 2.3-2.5** in parallel: warm-only admissions, bounded.
- **Phase 1.3b** the gap disassembly: 29,120 B, 5,456 B and 4,524 B
  runs are unstarted.
- The **Phase 3 / P3-P4** decomposition downstream of `fa_player` is
  the next bulk-boundary work. Infer_structs, frontier v2, taint, and
  decompose-next-block are all in place; the trigger is a measured
  scenario, not a tool.
- **Phase 4 / simulation systems** is the largest remaining bulk
  (hitboxes/hurtboxes, collision, damage/combos, ring-out, fighter
  physics, CPU logic). Fully unstarted.