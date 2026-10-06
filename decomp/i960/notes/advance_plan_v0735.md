# v0735: advance plan — three substantial slices in dependency order

Current state: `master` is at `ef96eb89`, fully synced with `origin/master`.
The repo has Phase 0-1.3 closed, Phases 2.1/2.2/2.6 (F4) closed, and the
factory layer for Phase 3 in place. The "considerable advance" path
below is what I would do next, in dependency order, with each slice's
estimated scope and acceptance.

## Slice 1 — Phase 2.5 ctest audit (smallest)

**What it is.** The six refused edit paths
(4505/4506/4509, 4625, 4634, 4636) all refuse at the `0xa6c0` gate by
design. v0734m documented it; the slice is the audit that proves it.

**Scope.** ~50 lines of Python wrapping `vf2probe` and
`vf2i960 native-resume`. For each of the six patterns:

1. Build a snapshot with the rejected input combination patched into
   the input latch at `0x500700`.
2. Run native to `0xa010`. Assert instruction count differs from any
   admitted shape.
3. Negative control: a known-admitted pattern (e.g. row-2 KICK at
   `0x0f000004`) runs and produces the published count.

**Acceptance.** New ctest entry `vf2_phase_2_5_refused_audit`,
all six refuse, both legs agree on each refuse, total wall < 60 s.

**Why first.** Smallest bounded slice, gets a Phase 2 entry off the
plan with low risk.

## Slice 2 — Phase 3 / P3-P4 first decomposition

**What it is.** `fa_player` recovered to the 6th dispatch entry;
downstream (P3 onward) is original i960. The factory layer
(`frontier v2`, `infer_structs` dual-base, `taint`) is in place but
waiting on a measured scenario.

**Scope.** Probably 200-400 LOC of recovered C, plus 1-2 ctest entries.
The shape:

1. Pick the **next call target** downstream of the 6th dispatch entry
   that the factory layer has flagged (`taint.py` on the 6th-dispatch
   trace identifies the highest-confidence callee).
2. Build a vf2probe scenario at the 6th-dispatch exit that drives
   the recovered C through that callee.
3. Re-run the differential; FULL MATCH at the callee boundary.
4. The recovered function appears in `src/recovered/` with the
   `recovered-control-block` or `recovered-observed-branch` status.

**Acceptance.** Native FULL MATCH at the callee boundary for at
least one new function. `block_coverage.py` updates the published
percentage.

**Why second.** This is the natural next bulk-boundary slice; the
tooling is in place and only needs a measured scenario.

## Slice 3 — Phase 4 first chunk (simulation systems)

**What it is.** Hitboxes / hurtboxes is the smallest simulation
subsystem. Per `fa_player_1442c_state16_state25_walker_miss_v0450.md`,
the relevant dispatching at `0x1442c` selects one of 5+ type-5 record
walkers; the first walker (state16/state25) is already recovered
there. Other simulation systems: collision detection, ring-out,
fighter physics, CPU logic.

**Scope.** Likely 500-2000 LOC of recovered C, several ctest entries.
The shape:

1. Pick one simulation subsystem (recommend **fighter physics** first:
   bounded movement, jump, crouch — measurable from the attract-mode
   player state).
2. Build a vf2probe scenario that drives a fighter through one
   measurable physics state transition (e.g. crouch-on-frame-N).
3. Recover the function. Re-measure on the scenario.
4. Repeat for two adjacent transitions (idle → crouch, crouch →
   idle).

**Acceptance.** A new subsystem is FULL MATCH on at least one
transition; the published percentage moves.

**Why third.** It is the largest remaining bulk and the user-visible
"decomp goes further" effect, but it is bounded by the same
factory-chain tools used for Phase 3.

## Slice ordering rationale

| slice | est. LOC | est. commits | est. wall |
|---|---|---|---|
| Phase 2.5 ctest | ~50 Python | 1 | < 1 h |
| Phase 3 first decomposition | ~200-400 C + Python | 2-3 | 1-2 days |
| Phase 4 first chunk (physics) | ~500-2000 C + Python | 4-8 | 1-2 weeks |

The dependency chain is **2.5 → 3 → 4**, but **2.3 and 2.4 are
independent** and could run in parallel with 3 and 4 if more bandwidth
were available.

## Why not Phase 2.3 or 2.4 first?

- **2.3** (+0x110/+0x114 setbits) needs a **live leg fixture that
  fires them**. The warm leg never does, so the fixture does not
  exist on disk and must be built from scratch. That is a measurement
  slice that may yield **nothing recoverable** (the live leg might
  fire different setbits than the warm one, requiring the gate to be
  widened without a C-level explanation). Risk: 1-2 days of work
  that may end at "still warm-only".

- **2.4** (7-instruction `a5=4` delta) needs two `vf2probe --trace`
  runs and IP-precise comparison. The cause is documented as "still
  unknown" in the recovered code. Risk: the cause may be a single
  `bno` on a register whose derivation requires three more traces;
  cost is bounded but may not yield a C-level explanation either.

Both are worth doing, but they are *measurement* slices that do not
move the published coverage. Slices 1, 2, 3 above move coverage.

## What this plan should NOT claim

- **No CPU-state simulation.** The plan recovers C code; the
  simulator side (the i960 reference executor) is already complete.
- **No ROM rewrite.** Nothing in this plan touches `roms/`.
- **No semantic naming without a second source.** Field names stay
  as `field_xxxx` until two independent measurements agree (per
  AGENTS.md).

## Validated

- `master` at `ef96eb89`, fully synced with `origin/master`.
- `git status` empty.

This plan does NOT commit code; it is a planning note. Execution
follows the slice order.