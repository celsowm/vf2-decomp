# v0739: Phase 2.5 fail-open investigation — c3a silent admission is a real recovery hole

This slice promotes the c3a silent admission from "documented but not
failed" to "**failing** ctest entry". The test gate now refuses to
admit `f2-r3-c3a-e2.vf2snap` and `f2-r3-c3a-e4.vf2snap` as
admitted — they are 4422/41 and 4421/41 respectively where the
reference produces 4636 and 4637.

## The bug, plain

The recovered `execute_frame_phase17_bit7_index5` admits the post-edit
release with the wrong body count for the c3a shape. The reference
ROM produces **14 more instructions** for the c3a edit than for the
admitted shape (4422 + 14 = 4636). The recovered C does not model
the 14-instruction delta.

## What the c3a snapshot carries (measured)

`f2-r3-c3a-e2` reads `preset = 0` at `0x00500324` and
`credits[3] = 3` at `0x00500329` (per v0732c). The recovered code
sees `preset = 0` (the post-release value) and falls through to the
admitted shape at `4422/41`. The reference ROM, in the same shape,
produces `4636`.

`f2-r3-c3a-e4` reads `preset = 0` and `credits[3] = 3`, the
post-edit release with `preset = 4` (per the recipe). Reference
`4637`, recovered `4421` — same 14-instruction delta.

## Why the gate does not catch this

The gate at `src/recovered/texture_bridge_match.c:8265-8291` checks:

- Line 8270: `(!post_edit_release && preset != 0u)` — refuses
  non-release path with preset > 0
- Line 8277-8281: `post_edit_release && (a5 == 4 ? preset > 2 :
  preset != 0u)` — refuses release path with preset > 0

**Both predicates assume the recovered code can see `preset > 0`.**
But after the test_held release, the workram at `base + 0x3324` is
back to 0. The recovered code reads `preset = 0` and falls through.
The reference, in the same shape, somehow produces 14 extra
instructions — this is the unmodelled delta.

## Three candidate fixes (none without a second source)

1. **Gate on `credits[3] > 3`**: the c3a shape has credits[3]=3
   but a non-zero **edit preset** that is **not visible at `base +
   0x3324`** after release. The recovered code does not see this
   value, so the gate cannot use it. **Not fixable from the recovered
   layer alone** without also modelling the workram scratch that holds
   the un-released preset.

2. **Tighten the post_edit_release predicate on a5=2/3 to also
   refuse when credits are non-standard**: this would refuse the c3a
   case, but it would also refuse any other post_edit_release on
   rows 2/3 with non-standard credits. Per `f3_punch_kick_release_measured_v0732.md`,
   the only admitted post_edit_release shapes on rows 2/3 are the
   `credits[3] == 3` (MATCH) ones — so a "credits != 3" check would
   specifically catch the c3a fail-open without affecting the
   admitted ones. **One-source rule applies**: this would close the
   hole but the admitted `credits[3] == 3` MATCH row is the same
   data source the c3a fail-open came from, so this is **a single
   source, not two**.

3. **Recover the 14-instruction ROM body that produces the delta**:
   trace the c3a case through `0x164c4` with `--trace` and find the
   instruction-level cause. ~1 day of work. **No second source
   needed** if the trace shows the ROM doing something the C does
   not.

## What this slice does NOT do

- **No `src/` change.** Tightening the gate without a second source
  would be a single-source repair, which `AGENTS.md` forbids.
- **No c3a recovery.** The 14-instruction ROM body is unmeasured
  on this build.
- **No fix to `f2-r3-edit.vf2snap` (the admitted control).** That
  fixture is the right reference and is correctly handled.

## What the next slice should pick up

- **Trace `f2-r3-c3a-e2` from `0x9ff8` to `0x164c4` with
  `vf2probe --trace --memory-trace`.** Find the 14-instruction delta
  between the c3a body (reference) and the admitted body
  (recovered). Likely culprits: a single `cmpobl` / `bno` on a
  register that the recovered code does not pin; or a register-clobber
  that requires restoring one more Model 2A word.
- **Once the delta is traced**, write a small C block that models
  the 14-instruction delta. Two-source rule: the trace AND the
  reverse-engineered block must agree.
- **Then the gate can be tightened** without a single-source risk:
  the recovered block + the tightened gate together cover the c3a
  case correctly.

## Validated

- `ctest -C Debug -R vf2_phase_2_5_refused_audit`: **FAILS** (1.12 s).
  Two cases (`f2-r3-c3a-e2`, `f2-r3-c3a-e4`) report silent
  admission. The test is now a failing gate, not a passing audit.
- `git diff --stat src`: empty (no C change).
- `git diff --stat decomp/i960/functions.csv`: empty.
- The other 7 refused paths still refuse; the admitted control
  (`f2-r3-edit.vf2snap`) still admits at 4637. The gate is correctly
  failing on the c3a hole.

## What this slice IS

This is a **failing ctest entry** that pins the c3a silent admission
as a regression. The next slice that touches `phase17_bit7_index5`
will see this gate fail and know to investigate.