# Completion plan — what "full decomp" still needs, in dependency order

Written after v0733e. This is a **plan**, not a recovery slice: nothing in it has
been implemented. Every phase states its acceptance criterion so it can be closed
without re-litigating.

The ordering is not by impact, it is by **dependency**. Phase 0 gates the
credibility of everything after it.

---

## Measured starting point

| metric | value |
|---|---|
| `functions.csv` rows | 101 (all labelled `recovered-*`) |
| rows with a real range | 69 |
| merged union | 316,664 B |
| **overlap double-counted** | **267,628 B** — the rows nest almost entirely |
| gaps inside the 445,020 B known span | **128,356 B** in 6 pieces |
| largest gap | `0x4d2c0..0x640f4` — **93,748 B, no CSV row** |
| rows still carrying a container bound (>20 KB) | 7, largest 269,476 B |
| `VF2_ERROR_UNSUPPORTED` return sites | 1,132 |
| recovered C | 83,339 lines |
| ctest entries | 119 (plus 3 excluded `4505` dominators) |

**Labels are not coverage.** The CSV claims 316 KB, but 267 KB of that is
overlap, and 13 rows still carry region bounds instead of extents. The
*characterised* surface is materially smaller than the CSV implies.

---

## Phase 0 — Stop the bleeding: oracle integrity

**Nothing else is trustworthy until this lands.**

### 0.1 B45 — the two drivers diverge on condition state

Reproduced (v0733e): over the same 7 instructions, from the same snapshot, in the
same binary,

```text
hand-rolled vf2_i960_step loop : bbs -> cc equal -> none  ... entry cc=none  arith=0x3f001000
vf2_i960_run (stop_address)    : ...                        entry cc=equal arith=0x3f001002
```

Same trace, same addresses, same `executed_instructions`, same registers.

**Steps**

1. Add a per-instruction `compare_result` / `arithmetic_control` dump to both
   drivers over the 7-step window; find the first instruction where they part.
2. Note the strong hint already measured: `bbs 5, r15` at `0x221f0` has bit 5
   **clear** (`r15 = 0x8A00` from `ld 0x00508000, r15`), and
   `executor_arch.c:150-160` maps a clear bit to `NONE`. So the hand-stepped loop
   is the arch-correct one and **`vf2_i960_run` is the unexplained side** — the
   path used by `vf2probe` and every differential tool.
3. `executor_arch.c` pre-evaluates compare operands and then calls
   `vf2_i960_step_legacy`, which decodes and executes the instruction **again**.
   Confirm whether that double execution is the cause, and whether
   `vf2_i960_run`'s uninitialised `vf2_i960_trace_event event` feeds
   `event.ip_after == event.ip_before` into the `stop_on_self_branch` check.
4. Fix whichever side is wrong. If it is `vf2_i960_run`, re-verify every
   committed compare-state pin.

**Acceptance:** a test that drives both paths over a fixture window and pins the
**difference** — the legacy path leaves the compare word alone where the arch
path rewrites it — plus a note naming the mechanism.

> **SUPERSEDED by v0733f and v0733g.** The mechanism is isolated: it is the
> `vf2_i960_step=vf2_i960_step_legacy` compile definition on `src/i960/executor.c`,
> so `vf2_i960_run` never applies `arch_fix_direct_compare`. The variant tree
> (`-DVF2_LEGACY_STEP_IN_RUN=OFF`) was built and measured: **19 of 119 fail, all
> on `compare_result` only.** The macro is therefore load-bearing for the current
> recovery and stays. The original "assert they agree" acceptance criterion is
> withdrawn — it could only pass if the whole recovery were re-derived.

**Risk:** if `vf2_i960_run` is the wrong one, a number of committed pins move.
That is the correct outcome, not a reason to defer.

### 0.2 Close the sanitizer blind spot

MSVC ASan does not catch stack-use-after-return. If any Phase 2-4 change touches
the runtime, executor, snapshots, memory or hardware modelling, that class is
currently uncovered. Decide explicitly: accept and document, or add a second
instrumentation path (clang-cl, or a manual poisoning harness).

**Acceptance:** a written decision in `AGENTS.md`, not an omission.

---

## Phase 1 — Bookkeeping correctness

Purely mechanical, fully specified, and it makes the rest measurable. Each step
is independent and can be done in any order.

### 1.1 Fix the extent oracle — **DONE at v0734a, with a different answer**

> **Closed by measurement, not by the plan's assumption.** The premise was wrong:
> the tool does **not** walk to a first `ret`. It builds real basic blocks and
> follows direct branch targets **including backward ones** — six texture entries
> all converge on `end=0x4bfe0` through a shared epilogue, and
> `main_texture_orchestrator_call` at `0xa030` owns 17 blocks, 11 of them *below*
> its entry via `b 0x9fb0`. A "real multi-exit extent analysis" already exists.
>
> Measured definition (now documented in `docs/I960_ANALYSIS.md`): `end` is one
> past the highest `ret` reachable through direct branches and fall-through; it
> does not descend into callees and does not follow indirect branches. So it is a
> sound lower bound, exactly as v0733c/v0733d concluded — only their stated
> mechanism was imprecise. The `0x23524` body is entered by `bal 0x23694`, which
> is the one case that matters, and `0x2364c` being *also* a callee address is a
> coincidence.
>
> `src/analysis/cfg.c` was left **byte-identical**: an experimental
> call-as-boundary change moved `basic_blocks` 3758 -> 4176 and **zero** extents.
>
> **The actual Phase 1 defect is bigger and was not on this list:** 25 of the 94
> bounded CSV rows carry a *call-return continuation* in `end`, not an extent,
> and were being deleted from every report. That is now 1.2b below, and it
> invalidates the arithmetic 1.3 depends on.

### 1.2 The container rows, one at a time

Rule from v0733c: repair a bound **only** when a second source agrees, and record
the provenance in the row's `notes`. Do not bulk-rewrite from a lower bound.

- **1.2a** — the **13 remaining container rows**. Each needs its own measured
  extent; the v0733c table lists them with their tool answers.
- **1.2b (new, v0734a)** — the **25 inverted rows**, which are not currently in
  the table at all because they are silently dropped. All 25 true extents are
  ROM-measured in `function_extent_measured_v0734a.md`. **Do not "repair" them by
  guessing**: an inverted row is a *different quantity*, not corrupt data, and
  the correct fix is a schema change (a separate `return_to` column), not a
  rewritten `end`. Three of the 25 are `indirect=yes` and are lower bounds only.

**Acceptance:** zero rows whose span is a region bound; each repaired row names
its two agreeing sources; no row is dropped without being named.

### 1.3 Characterise the gaps

- `0x4d2c0..0x640f4` — 93,748 B, no CSV row. The single largest unknown in the
  table; likely several functions, not one.
- `0x00001200..0x00001290` — 144 B, exposed by v0733c.
- `0x000012bc..0x000012d8` — 28 B, same.

**These three figures were computed over a table in which 25 of 94 rows were not
ranges (v0734a). Recompute the gap list after 1.2b before trusting any of it.**

**Acceptance:** each gap has at least a disassembly pass and a statement of what
it contains. Characterisation is the deliverable, not recovery.

### 1.4 Make the coverage tool certify, not just rank

`block_coverage.py` reports `coverage_ratio = 0.00` for every ranked row: the
v0729-v0732 corpus (`trace-both.jsonl`, `trace-f0.jsonl`) never reaches those
addresses. The tool currently **orders candidates and certifies nothing**, which
is easy to over-read.

**Acceptance:** either a corpus that reaches the ranked addresses, or the tool
prints a prominent "no coverage data for these addresses" instead of a silent
`0.00`.

---

## Phase 2 — Close the named fail-closed admissions

Each is a *known* non-divergence with a measurement already in hand. Work them in
the order listed; each is independent.

| # | item | evidence in hand | notes |
|---|---|---|---|
| 2.1 | `g14` at the `0x23524` return | `0x23648` on both admitted legs, entry value `0x22428` on the refused one | path-dependent via the `bal` at `0x23644`; no single constant is correct |
| 2.2 | live-leg condition state | reference `GREATER` / `0x3f001001`, native publishes warm `EQUAL` / `0x3f001002` | a **real admitted-leg defect**; needs the live path's last compare-setting instruction |
| 2.3 | `+0x110` / `+0x114` setbit sides | warm leg only | widen with the neighbouring leg, never from one sample |
| 2.4 | 7-instruction `a5=4` delta | measured, uncharacterised | |
| 2.5 | edit paths 4505/4506/4509, 4625, 4634, 4636 | all currently refuse | |
| 2.6 | **F4** — INDIVIDUAL walks rows 1-2 | bounded and measured (`f3_f4_individual_mode_measured_v0732.md`) | last open TEST MENU item; rows 3-4 are unreachable by design |

**Acceptance per item:** a ROM-backed differential exists that FULL MATCHes on
the admitted shape, **and** the neighbouring refused shapes are pinned with their
measured reference values so a widened gate fails.

---

## Phase 3 — The bulk: P3-P4 downstream of `fa_player`

The corridor recovered so far is boot → scheduler → menus → player dispatch →
collision-shell plumbing. **P3-P4 has not started.**

The factory tooling is built for exactly this and is the intended path:
`make_game_info_probe_scenario` → `sweep_state` / `explore_state` →
`infer_rules` → `infer_structs` → `frontier.py` → `taint.py` →
`trace_case` / `minimize_case`.

**Steps**

1. Run `infer_structs` across the dual-base player corpus and promote
   `field_0980`, `field_0984`, `field_11a0`, `field_1680`-`1688` to
   multi-corridor provenance.
2. Use `frontier.py` v2 to find the next uncovered edge that touches them.
3. `taint.py` to capture which branch depends on them.
4. Decompose the next recovered block, one at a time, each with its own
   differential.
5. Never unblock `0x27cc8` with a saturating `cvtri` rule — the i960 leaves
   out-of-range FP→int undefined and the refusal is correct.

**Acceptance:** the next player block is FULL MATCH differentially, and the
struct promotion is backed by `base_count == 2` evidence rather than assertion.

---

## Phase 4 — The simulation systems

This is the bulk of "full decomp" in any meaningful sense. **None of it is
started**, and none of it may be written from game knowledge — every field and
every rule has to come from observed state transitions and memory access
patterns.

Suggested order, by what the tooling can currently support:

1. **hitboxes / hurtboxes** — the most-accessed shared fighter offsets make it
   the best first target; `infer_structs` + `taint` should light it up.
2. **collision** — partially in the shell already; the remaining arms are the
   resolver cascade.
3. **damage / combos** — downstream of contact results.
4. **ring-out behaviour** — boundary conditions, cheap to bound once the
   position fields are named.
5. **fighter physics** — the largest single system.
6. **CPU decision logic** — largest and hardest; last.

**Acceptance per system:** provisional layout with evidence-backed fields, a
recovered code path, and a differential proving it against the reference at a
controlled boundary. No semantic naming ahead of that.

---

## Phase 5 — Semantic naming (last, evidence-gated)

`field_xxxx` stays until independent evidence supports a name. Renaming to
`health`, `animation_state` and so on is **not** allowed on plausibility, and the
`0x0d00` block deliberately has no name at all — it is a render/pose/animation
descriptor, not a gameplay-state field (v0730/v0732m).

**Acceptance:** every rename cites a second, independent source — static
addressing, a caller, or a differential — not a single inference.

---

## Cross-cutting rules for every phase

- **Strict gate stays green**: `cmake --build build --config Debug --parallel`
  then `ctest --test-dir build -C Debug --output-on-failure`.
- **Phase 0 and any runtime/executor change** additionally run the sanitizer
  tree (`-G "Visual Studio 17 2022" -A x64`, ASan bin dir on `PATH`).
- **The most specific ROM-backed differential** exercises the change; a build
  that compiles is not a recovery.
- **Negative controls on neighbouring shapes**, not just the admitted one.
- **A gate must be provable able to fail.** Plant the defect, watch it fail.
- **Never commit** `out/`, `.vf2snap`, JSONL traces, or scratch scripts.
- **Retract loudly and in place**, naming which claims are withdrawn and which
  are retained — v0733e is the model.
- The three `4505` ctest entries are a ~23-minute-per-entry dominator; run them
  separately from the rest to stay inside command timeouts.