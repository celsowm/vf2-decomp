# AGENTS.md

This file is the operational handoff for coding agents working on `vf2-decomp`.
It is intentionally more prescriptive than the README. Read it before making
changes, then read the focused recovery documents for the subsystem you touch.

## Mission

`vf2-decomp` is a clean-room, non-matching C17 recovery of **Virtua Fighter 2
Version 2.1** for Sega Model 2A / Intel i960.

The goal is **portable recovered behavior**, continuously proven against the
original i960 program. The reference executor is an oracle and exploration tool;
it is not the desired final implementation.

The repository contains no ROMs. Never add ROM data or derived proprietary
artifacts to Git.

## Non-negotiable rules

1. **Evidence before implementation.** Do not invent game semantics, object
   layouts, names, branch conditions or hardware behavior.
2. **Fail closed.** Unverified paths must remain `VF2_ERROR_UNSUPPORTED` or an
   explicit ROM-backed boundary. Never make an unknown path silently succeed.
3. **The original i960 execution is the oracle.** A plausible implementation is
   not accepted until it matches measured reference behavior at a controlled
   boundary.
4. **Preserve exact state where the differential contract requires it.** This may
   include registers, condition state, local frames, call/return counters,
   scheduler state, mutable Model 2A memory and modeled device-visible state.
5. **Do not weaken validation to make a recovery pass.** Fix the recovery or
   improve the evidence instead.
6. **Keep the Model 2A oracle behavior stable.** Instrumentation must be passive.
   Observers may record successful accesses; they must not decide hardware
   behavior or mutate state.
7. **Generated pseudocode is navigation only.** Never copy generated pseudo-C
   wholesale into `src/recovered/` and call it recovered.
8. **No proprietary artifacts in commits.** In particular, do not commit ROMs,
   reconstructed ROM regions, `.vf2snap` files, large/full traces, extracted
   textures/models/audio, or generated pseudo-C derived from the ROM.
9. **Prefer the smallest proven semantic change.** Broad speculative rewrites
   make differential debugging much harder.
10. **When uncertain, preserve the boundary.** Unknown is better than wrong.

## First files to read

Start with these, in this order:

- `README.md` — project overview, build and top-level tools.
- `docs/UNCOVERED_BRANCHES.md` — current native/ROM-backed frontier.
- `docs/NATIVE_DIFFERENTIAL.md` — exact validation contract.
- `docs/NATIVE_RUNTIME.md` — recovered runtime architecture.
- `docs/DECOMP_GUIDE.md` — recovery lifecycle and evidence conventions.
- `docs/PROBE_AUTOMATION_PLAN.md` — automated probing/exploration workflow.
- `decomp/i960/notes/` — address-level evidence.
- `CHANGELOG.md` — useful context for why a boundary exists.

Do not trust a status sentence in this file over newer measured evidence. If the
repository has advanced, update this handoff as part of the same work.

## Windows environment

The canonical agent workflow on this checkout is **native Windows CMake + MSVC**.
The committed `build/` directory is configured with the Visual Studio generator,
`VF2_BUILD_TESTS=ON`, `VF2_WARNINGS_AS_ERRORS=ON` and
`VF2_ROM_DIR=<repo>/roms/vf2`, so it is the primary ROM-backed differential and
strict-test validation path.

Run build and tests from the repository root with:

```powershell
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
```

MSBuild is invoked through `cmake --build` (the VS generator resolves it), so no
separate developer prompt is required. Python recovery/analysis tooling also runs
natively under `python tools/python/...`; adapt the documented `build/...` binary
paths to the native `build\Debug\...` output layout (for example
`build\Debug\vf2probe.exe`).

A WSL2 `build-wsl/` directory remains available as a secondary cross-check and is
kept in sync when the user requests it, but it is not the default path. Behavior
must stay aligned with the documented strict build/test gate and the ROM-backed
commands described throughout this handoff.

## Build and test gate

Normal strict build:

```sh
cmake -S . -B build \
  -DVF2_BUILD_TESTS=ON \
  -DVF2_WARNINGS_AS_ERRORS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

With a legally obtained supported ROM set:

```sh
cmake -S . -B build \
  -DVF2_BUILD_TESTS=ON \
  -DVF2_WARNINGS_AS_ERRORS=ON \
  -DVF2_ROM_DIR=/path/to/vf2
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Sanitizer gate for changes touching runtime, executor, snapshots, memory or
hardware modeling:

```sh
cmake -S . -B build-san \
  -G "Visual Studio 17 2022" -A x64 \
  -DVF2_BUILD_TESTS=ON \
  -DVF2_WARNINGS_AS_ERRORS=ON \
  -DVF2_ENABLE_SANITIZERS=ON \
  -DVF2_ROM_DIR=/path/to/vf2
cmake --build build-san --parallel
ctest --test-dir build-san --output-on-failure
```

**The generator and the `PATH` are not optional (v0732s).** Until v0732s the
MSVC branch of `VF2Warnings.cmake` never applied any sanitizer flag, so
`VF2_ENABLE_SANITIZERS=ON` instrumented nothing while reporting a pass — the gate
could not fail. Two consequences on this machine:

- the default `cmake` resolves to **VS 18 BuildTools 14.50.35717**, whose toolset
  ships `clang_rt.asan*` for arm64/i386 only, so `/fsanitize=address` dies at
  link with `LNK1104`. Use the **VS 2022** generator above, which has the x64
  libraries. A `build-san/` pinned to the other toolset must be **removed and
  recreated**, not reconfigured in place.
- the instrumented binaries exit `0xC0000135` until the toolset bin directory is
  on `PATH`, and **ctest hangs** rather than failing:

```powershell
$env:PATH = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64;" + $env:PATH
```

`CMakeLists.txt` now prints which instrumentation is in effect at configure
time. **Check that line before believing a sanitizer result.** Do *not* use
wall-clock as the check: once armed, this suite costs only ~8% more under ASan
(1497 s vs ~1380 s of CPU on the dominating test), so a "sanitizer runs should be
much slower" heuristic would report a working gate as broken. The reliable tells
are the configure-time line and the binary's dependency on the sanitizer runtime
(an uninstrumented binary cannot exit `0xC0000135`). See
`decomp/i960/notes/sanitizer_gate_was_a_noop_v0732s.md`.

A change is not finished merely because it compiles. Run the most specific
ROM-backed differential path that exercises the new recovery.

## Current handoff status

At the time this handoff was written, `master` already contains:

- a substantial native boot/runtime/scheduler corridor;
- repeated native dispatch through the fifth and sixth gameplay entries;
- the current accepted `fa_game_info` and `fa_player` corridors described in
  `docs/UNCOVERED_BRANCHES.md`;
- a reference i960 decoder/executor with snapshot/resume support;
- strict recovered-vs-reference differential tooling;
- `vf2probe` for machine-readable controlled experiments;
- declarative state sweeps and measured rule inference;
- coverage-guided guest-i960 state exploration and testcase minimization;
- Model 2A memory-access tracing; and
- candidate fighter/object field inference from repeated `base + offset`
  accesses;
- the COIN ASSIGNMENT menu corridor (selector 17 index 5) recovered natively
  from TEST MENU: TEST press, held entry, release, idle, full parent walk,
  row-1 mode/edit cycle in COMMON and INDIVIDUAL modes, rows 2/3/4 pre-edit
  renders, EXIT+ park and EXIT- redraw (v0727);
- the state-8 bit-6 symmetric matrix proven 6144/6144 native-exact with the
  triple/quad table folded into a counted rule (v0728); and
- the v0729 factory layer: `frontier.py v2` (per-edge fighter offsets +
  per-source attribution + fighter-aware rank, contiguous-fighter-blocks
  detector, `--json` surface; 18/18 tests), `taint.py` unit suite
  (7/7), taint end-to-end contract on the real `out/trace-bit14.jsonl`
  corpus (21 branch blocks, 24 fighter dependencies, all matching the
  `fighter + 0xNNNN [bit N]` shape), `infer_structs.py` dual-base
  promotion unit suite (4/4), `infer_rules.py` conservative-refusal
  unit suite (10/10), `check_scenario.py` validation gate (9/9),
  `sweep_state.py` / `explore_state.py` / `minimize_case.py` /
  `trace_case.py` / `z3_branch.py` unit suites
  (9/9 / 9/9 / 7/7 / 4/4 / 5/5 skipping without z3), `test_factory_chain.py`
  integration (4/4) chaining Step 1 → Step 2a → Step 2 + infer_rules,
  `factory_chain_demo.py` runnable PASS exemplar,
  `p1_real_trace_demo.py` real-corpus factory chain demo (ctest
  entry #117; reproduces the 0x1680..0x1860 contiguous 4B block
  evidence on the existing `out/trace-both.jsonl` +
  `out/trace-f0.jsonl` corpus with all invariants locked), the 14
  `vf2_python_factory_*` ctest entries wired into `CMakeLists.txt`
  (the 14th is `vf2_python_factory_block_coverage`, the v0733
  per-function coverage tool described in
  `decomp/i960/notes/block_coverage_factory_v0733.md`),
  the factory runbook note `decomp/i960/notes/factory_runbook_v0729.md`,
  the v0729 player-corpus smoke + 0x1680 contiguous-block notes,
  the v0729 session close-out note, the v0730 first-action runbook
  (`decomp/i960/notes/v0730_first_action_runbook.md`) as the single
  handoff entry point for the next agent, the F1 starting-state note
  (`f1_manual_setting_entry_boundary_v0730.md`: cursor dispatch at
  0x59f34 + row-4 handler at 0x5a0a4 + slice pickup), and the P1
  first/second notes
  (`p1_player_0x1680_block_stability_v0730.md` +
  `p1_taint_0x1680_block_v0730.md`: block is structurally stable but
  does NOT feed fighter-flag branches — render/pose/animation-state
  descriptor, not a gameplay-state field) (102 Python test cases, ~7.4 s
  wall). The proven native dispatch boundary was re-measured at v0730
  from `out/sixth-fresh.vf2snap` to `0x164c4` in 14,277,453
  instructions with 10,288 calls / 10,286 returns, captured in
  `decomp/i960/notes/native_dispatch_boundary_v0730_measured.md`.
- the v0732b-v0732g TEST MENU / COIN ASSIGNMENT slice: rows 2-4 post-edit
  release frames recovered in COMMON (4422/4421/4418 at 41 calls, with the
  body length as a *counted* singular-label rule rather than a table), three
  ROM-grounded corrections ((6,40) is `credits[1]` not `credits[0]`; row 24's
  chute slot is `0x61550 + 2*preset`; `a5=4` splits 4193/4186 on
  `preset == 0`), the INDIVIDUAL mode leg built from scratch with no patched
  state, and the INDIVIDUAL value-row release **recovered** at
  `4062 - singulars` / 32 body calls (three natural frames full-match; rows
  3-5 refused). See `f2_post_edit_release_recovered_v0732.md`,
  `f3_row3_release_count_rule_v0732.md`, `f3_row6_credit1_correction_v0732.md`,
  `f3_chute_slot_and_count_correction_v0732.md`,
  `f3_f4_individual_mode_measured_v0732.md` and
  `f3_individual_value_row_recovered_v0732.md`.

The most recent tooling layer is intentionally **above** the validated executor.
It accelerates evidence gathering; it does not replace the oracle.

## Important current gameplay frontiers

Always confirm these against `docs/UNCOVERED_BRANCHES.md` before coding.

### `fa_game_info` around `0x00018644`

A large state-4/state-8 matrix is already recovered. State-8 bit 6 is exact for
the complete negative-threshold matrix and for the complete 512-composition
symmetric positive-threshold matrix (v0728 sweep: 6144/6144 native-exact).
**Unilateral mixed positive bit-6 compositions outside the symmetric sweep
remain explicitly unsupported.**

Do not add another giant hand-written mask table unless the measured behavior
really requires one. The v0728 work is the reference example of the intended
flow (it retired the 15-entry triple/quad table this way). Prefer:

1. generate a measured scenario;
2. sweep or explore it;
3. cluster outcomes;
4. infer a compact candidate rule;
5. translate that rule to C; and
6. prove every accepted combination differentially.

### `fa_player`

The accepted sixth-entry player path now reaches well into the downstream
player/geometry chain and returns through both fighter task records. Later
player branches remain original-i960 continuations.

High-value missing gameplay still includes fighter physics, collision,
hitboxes/hurtboxes, damage/combos, ring-out behavior and CPU decision logic.
Do not attempt to implement those systems from game knowledge. Recover them from
observed state transitions and memory access patterns.

### Portable fighter/object structures

This is now a practical target, but field names must remain evidence-backed.
It is acceptable to introduce provisional layouts such as:

```c
struct vf2_fighter_candidate {
    /* ... */
    uint32_t field_1a4;
    /* ... */
};
```

It is not acceptable to rename `field_1a4` to `health`, `animation_state`, etc.
until independent evidence supports that semantic name.

## Core tools

### `vf2i960`

Use for disassembly, static analysis, snapshots and native/reference milestones.
Typical commands:

```sh
build/vf2i960 disasm /path/to/vf2 0x00018644 128
build/vf2i960 function /path/to/vf2 0x00018644
build/vf2i960 analyze /path/to/vf2 out/analysis
build/vf2i960 native-fifth-dispatch /path/to/vf2
build/vf2i960 native-sixth-dispatch /path/to/vf2
```

### `vf2cycles`

Use to resume a proven snapshot and advance recovered/reference cycles. Strict
runs stop at the first unsupported native block, reference failure or mismatch.

```sh
build/vf2cycles \
  --rom-dir /path/to/vf2 \
  --snapshot checkpoint.vf2snap \
  --cycles 10 \
  --min-blocks 1 \
  --max-blocks 16384
```

Use `--boundary-probe` only for scouting where cycle-boundary equality is the
intended contract. Do not use it to hide a block-level mismatch.

### `vf2recover`

Use for a human-readable recovery report around a checkpoint. It is for analyst
inspection, not bulk machine processing.

### `vf2probe`

Use for reproducible machine-readable experiments.

It can:

- restore a `.vf2snap`;
- patch registers;
- patch `u8`, `u16` and `u32` memory;
- stop at a selected guest address;
- emit guest instruction trace records;
- emit Model 2A memory-access records;
- read selected final memory values; and
- save the resulting snapshot.

Example:

```sh
build/vf2probe \
  --rom-dir /path/to/vf2 \
  --snapshot checkpoint.vf2snap \
  --set-u32 0x00501234=0x100 \
  --until 0x000164c4 \
  --trace \
  --memory-trace
```

`--memory-trace` records **successful** Model 2A accesses. It is enabled only for
the reference run itself; scenario mutations, final inspection reads and output
snapshot capture are intentionally excluded.

Memory events use the absolute upcoming instruction step. The corresponding
`step` record emitted immediately after execution carries the exact
`ip_before`. Correlate by `step`; do not infer the instruction address from an
already-advanced CPU IP.

## Automated recovery workflow

### 1. Generate a measured `0x18644` scenario

Do not hard-code fake fighter addresses. Resolve them from the measured
`0x164ac` boundary:

```sh
python decomp/i960/tools/make_game_info_probe_scenario.py \
  build/vf2i960 \
  build/vf2probe \
  /path/to/vf2 \
  out/state8-positive.json \
  --state 8 \
  --bits 1,2,4,6,8 \
  --threshold 0
```

The generated scenario contains the actual live fighter pointers and mode
address measured from that snapshot.

### 2. Exhaustive sweep when the domain is bounded

```sh
python tools/python/check_scenario.py out/state8-positive.json
python tools/python/sweep_state.py \
  out/state8-positive.json \
  --output out/state8-positive.jsonl
```

Use exhaustive sweeps for domains small enough to prove completely. Do not
claim a complete rule from a sparse sample.

### 3. Infer candidate boolean rules

```sh
python tools/python/infer_rules.py \
  out/state8-positive.jsonl \
  --bitfield fighter0_flags:1,2,4,6,8 \
  --bitfield fighter1_flags:1,2,4,6,8
```

`infer_rules.py` is deliberately conservative. If selected features do not
uniquely determine the outcome, or the truth table is incomplete, it should
refuse to produce a minimized rule. Preserve that behavior.

A minimized expression is a **hypothesis**, not accepted recovered semantics.
Implement it in C only after inspecting the measured cases, then prove it
against the reference matrix.

### 4. Explore large domains by guest edge coverage

```sh
python tools/python/explore_state.py \
  out/state8-positive.json \
  --corpus out/state8-corpus \
  --iterations 10000 \
  --seed 1 \
  --max-mutations 3
```

Coverage is based on exact **guest i960 edges** `ip_before -> ip_after`.
Do not substitute host compiler coverage: AFL/libFuzzer-style coverage of the C
executor mostly measures the interpreter implementation, not distinct guest
program paths.

The corpus is resumable. Keep a candidate only when it adds a previously unseen
guest edge.

### 5. Minimize a discovered branch witness

```sh
python tools/python/minimize_case.py \
  out/state8-positive.json \
  out/state8-corpus/case-00017.json \
  --edge 0x00018bd4:0x00018c30 \
  --output out/minimized-18bd4-18c30.json
```

The minimizer repeatedly re-runs the reference executor. A mutation may be
removed only if the target edge remains reproducible.

### 6. Trace one exact case including memory

```sh
python tools/python/trace_case.py \
  out/state8-positive.json \
  --output out/state8-case.jsonl \
  --set fighter0_flags=0x40 \
  --set fighter1_flags=0x0 \
  --set countdown=0 \
  --set threshold=0
```

The trace is streamed directly to disk so large runs do not have to accumulate
in Python memory.

### 7. Infer candidate object fields

```sh
python tools/python/infer_structs.py \
  out/state8-case.jsonl \
  --scenario out/state8-positive.json \
  --json out/state8-fields.json
```

The analyzer groups candidate offsets by:

- object base(s);
- read/write frequency;
- access width;
- absolute addresses; and
- guest IPs touching the offset.

Offsets observed relative to both fighter bases are especially useful evidence
for a shared fighter layout.

The analyzer is streaming. Preserve that property: trace length may be very
large.

## Recommended next work

Unless newer evidence changes priorities, the following order gives the best
leverage. The v0729 factory layer (frontier v2, taint unit + E2E, infer_structs
unit, factory runbook note) is now in place; the next F/P1 slices are
expected to consume it via `decomp/i960/notes/factory_runbook_v0729.md`.
The single handoff entry point for the next agent is
`decomp/i960/notes/v0730_first_action_runbook.md`, which documents the
TL;DR build/test gate, what's on master, what's deferred (F1-F5 / P1-P4 /
S1), the factory chain composition paths for both F- and P-slices, the
ctest-failure recovery procedure, the reference reading order, and the
AGENTS.md anti-traps.

### 1. TEST MENU corridor — CLOSED except F4

F1 (MANUAL SETTING entry/teardown), F2 (rows 2-4 post-edit release), F3
(PUNCH/KICK value-row releases, both modes) and F5 (which was already closed -
the v0730 item was a false premise) are all recovered and proven. F3 closed at
v0732g: the INDIVIDUAL value-row release needed only a latch and a count, not
a new render.

**F4 is the one item left, and it is measured rather than open-ended.** In
INDIVIDUAL mode row 2's down-neighbour is row **0**, not row 3, and the step
also leaves INDIVIDUAL mode. So rows 3 and 4 are not reachable by walking
while in INDIVIDUAL, and the selection list wraps early. The concrete F4
targets are therefore INDIVIDUAL walks at rows 1-2 only. See
`f3_f4_individual_mode_measured_v0732.md`.

Remaining fail-closed pins in this corridor, all deliberate: the INDIVIDUAL
value-row release on rows 3-5; the INDIVIDUAL `a5 = 4` COIN/CREDIT SETTING
row; the MANUAL SETTING `a7` nested editor; and PUNCH/KICK at row 5.

### 2. Extend the player corridor downstream of `0x28918`/`0x29414`

**P1 is closed at v0732k — the dual-base promotion fired.** See the P1 status
block below before starting anything here. **P2 is re-scoped at v0732q** (its
original goal is void) and P3-P4 are the remaining decomposition work.

**P2 status at v0732q** (`p2_producer_contract_v0732q.md`):

- **The `0x0d00` block is the cluster of `coli_2396c_body`, which has been
  native C since v0285.** `0x2396c` is three setup instructions before the
  `0x23980` loop (`lda 0x020078a8, r3` / `ldob 0x04(g7), r15` /
  `ld 0x23944[r15*4], r8`). The v0729-v0732 line analysed the block as
  unidentified without first searching for an existing recovery of the same
  code. Do that search first next time.
- **v0732p's `w3 = 0` is RETRACTED.** `stq r4` stores the quadword
  `{r4,r5,r6,r7}` and `r7` is loaded from `r3 + 0x0c + slot*16`; the window is
  **ROM-resident** at `0x020078b4` and holds 30 distinct small positive floats
  in live state. 30/30 non-zero, 30/30 matching. The old fixture pinned a
  blank `r3 = 0x0100a000`, so every word read as zero.
- **The real gap was the missing differential, and it found two real bugs.**
  `test_coli_2396c_poly_cluster` synthesised an *identity* index table, never ran
  the reference executor and **zeroed the cluster** — the same condition that
  produced the retraction. `tests/recovered/test_coli_2396c_live.c` (v0732r) is
  the replacement: real ROM, real permutation, real 4th-word window, two legs
  differing only in `g7`, both FULL MATCH on 120 cluster words + 30 fourth words
  + registers + counts. It caught `g3` being dropped (native 0 vs reference
  `0x0000fffe`) and `g4` not being complemented (0 vs `0xffffffff`).
- **i960 three-operand forms print the destination LAST.** `cmpinco 29, r9, r9`
  leaves `r9 = 30`, so `r9` is the destination; therefore `and g4, g3, g3` is
  `g3 &= g4`, not `g4 &= g3`. Getting this backwards made a previously-correct
  recovery wrong and the differential caught it immediately.
- **Search the repo before re-deriving.** The existing body already held
  `0x2394c`, `0x0d00`, `0x23944`, `0x04`, `0x020078a8`, `+0.05f`, `-0.1f` and
  both float adds, all with correct comments. `docs/UNCOVERED_BRANCHES.md:4955`
  records the v0285 recovery.

The v0706 dual-base witness and v0707 head-sibling work recovered the
front of the `fa_player` downstream chain; later branches remain
original-i960 continuations. Layer 2 (`fa_player`) P1 in the runbook is:
run `infer_structs.py` on the dual-base player trace to upgrade
single-corridor candidates (`field_0980`, `field_0984`, `field_11a0`,
`field_1680`-`1688`) to multi-corridor provenance, then use frontier v2
to find the edge that touches them, then taint.py to capture the branch
dependency. P3-P4 chain the next downstream decomposition.

**P1 status at v0732k** (`p1_fighter_bases_retraction_v0732k.md`):

- **P1 is DONE. The block is dual-base and promoted.** The whole
  v0729-v0732 line had been using the WRONG fighter bases
  (`0x510000`/`0x520000`); the measured ones are **`0x510980`/`0x512980`**
  (from `out/state8-positive.json` metadata). The wrong fighter1 window did
  not contain the real struct, so all fighter1 traffic was invisible.
- `out/trace-both.jsonl` has **144 shared offsets, 341 events per base**.
  The contiguous 4B block is `0x0d00..0x0ee0`, length 120, 480 B,
  `ip_overlap` 1.0, IPs `0x2399c` + `0x23a38`, **240 reads + 240 writes per
  base**, `base_count == 2`. The v0729 note's `0x1680..0x1860` is the same
  block measured from base `0x510000`; the difference is exactly `0x980`.
- The v0730 taint note was **right**. Its `fighter1 + 0x01a4 bit 14` and
  `fighter1 + 0x05b8 bit 0` lines reproduce exactly with the correct bases.
  The caveat added at v0732h is removed.
- **A committed bug asserted the wrong answer.** `p1_real_trace_demo.py`
  (ctest #117) hardcoded the wrong bases *and* pinned `base_count == 1`, so
  the "pending promotion" was enforced by a green test. Fixed; the test now
  asserts `base_count == 2` plus the per-base read/write balance.
  **Full suite 117/117 in 1635.00 s with the corrected assertion live.**
- **The whole visible struct is dual-base, not just the block.** All 144
  offsets are shared, and every one of the top 40 by access count is
  `base_count == 2`. The promoted provisional layout is in
  `p1_promoted_dualbase_layout_v0732m.md`; `+0x1a4` is the flags word (70
  reads, 5 guest IPs, branches test bits 0/8/14/18/23) and `+0x0650` carries
  a sign-bit dependency, while the `0x0d00` block has no branch on it - both
  v0730 claims true at once.
- **Retracted:** v0732h ("corpora are fighter0-only", "no fighter1 trace
  exists", the taint caveat), v0732i ("disjoint regions", "nothing
  promotes"), v0732j ("0 of 264 traces share an offset" — **81 of 264 do**,
  "P1 is a state problem"). The v0732h `0x27cc8` `cvtri` analysis is
  **unaffected and still valid** — that boundary is real.
- **v0732i's multi-trace roll-up is kept** — a real capability, just not
  needed here. `infer_structs` takes `nargs="+"` traces with per-base
  `base_traces` provenance (unit suite 4 → 8).
- Do **not** unblock the reference executor past `0x27cc8` with a saturating
  `cvtri` rule. The i960 leaves out-of-range FP→int conversion undefined, the
  refusal is correct, and the C side already recovers the tail.

**Standing rule after this episode:** any analysis that names a fighter base
must cite where it came from, and a *pending* result must never be asserted
by a test — otherwise a wrong premise becomes a green gate. The only measured
pair in this repo is the scenario metadata's; `0x510000`/`0x520000` are not
fighter bases in VF2.

### 3. Extend `frontier.py`

`tools/python/frontier.py` v2 (v0729a) now provides per-edge
`fighter_read_offsets` / `fighter_write_offsets` /
`fighter_access_count`, per-source `sources` attribution and a
fighter-aware score bonus. Remaining extensions, in rough value order:

- caller-callee correlation when the edge source is a `call`
  instruction (already partial via `CALL_MNEMONICS`);
- DuckDB-backed persistence when corpus volume outgrows the streaming
  aggregator (already partial via the optional `--duckdb` /
  `--parquet` paths and the `test_duckdb_parquet_export` gate); and
- cross-corpus fighter-offset roll-up so multi-trace dual-base
  promotion is automatic.

Do not force these dependencies into the C runtime, and keep ranking
features strictly measured: no invented semantics enters the report.

### 2. Turn repeated memory patterns into candidate layouts

Run multiple minimized `fa_player` and `fa_game_info` witnesses through
`trace_case.py`, aggregate the repeated fighter offsets, and create provisional
structures only where the layout is stable across cases/fighters.

Prefer evidence such as:

- same offset from fighter0 and fighter1;
- same access width;
- repeated access from the same guest functions;
- consistent read/write role across state transitions; and
- independent static addressing evidence.

### 3. Add targeted dynamic taint

Do not begin with full symbolic execution. Start with taint for the operations
actually seen in the target corridor:

- loads;
- moves;
- bitwise operations;
- shifts;
- add/subtract;
- comparisons; and
- conditional branches.

The desired output is evidence such as:

```text
branch 0x00018698 depends on:
  fighter0 + 0x1a4 bit 6
  fighter0 + 0x5b6
```

Keep taint metadata outside architectural CPU state so it cannot influence the
oracle.

### 4. Use Z3 only after a concrete measured question exists

`z3-solver` is already an optional analysis dependency. Use 32-bit bit-vectors
for i960 integer semantics when a measured branch relation remains difficult to
simplify empirically.

Good use:

- solve a specific branch precondition;
- prove equivalence of two candidate bit-mask predicates over a bounded domain;
- generate missing witnesses for a measured branch.

Bad use:

- symbolically execute the whole game;
- replace differential evidence with a solver-generated guess;
- add a heavyweight symbolic framework that requires reimplementing i960 before
  it produces value.

### 5. Optional external static analysis

Ghidra can be useful as a second static analyst if using an Intel 80960-capable
build/processor module. Treat its decompiler output as suggestions only.

Principle:

```text
Ghidra suggests -> our executor measures -> differential tests prove
```

Do not make Ghidra, angr, Triton, Unicorn or Miasm a core runtime dependency.
The project already has an i960 decoder/executor; reimplementing i960 inside a
large framework is not the current bottleneck.

## Recovery lifecycle for one branch/function

For every new native recovery:

1. Identify the exact current unsupported/native boundary.
2. Disassemble the surrounding i960 instructions.
3. Inspect CFG/xrefs and any existing notes.
4. Reproduce the branch from a deterministic snapshot.
5. Minimize the input state when possible.
6. Record observable reads/writes and branch dependencies.
7. Extend i960 instruction semantics only if the reference executor is actually
   missing a verified instruction required by the path.
8. Write the smallest semantic C recovery.
9. Keep unobserved sibling branches unsupported.
10. Add a ROM-independent unit test where practical.
11. Add or expand the ROM-backed differential fixture/matrix.
12. Run strict build/tests and the focused differential command.
13. Update `docs/UNCOVERED_BRANCHES.md` and any focused evidence note.
14. Commit only source, tests and compact evidence descriptions — not snapshots
    or full traces.

## Differential acceptance checklist

Before calling a path native, ask all of these:

- Does the reference run reach the same boundary?
- Are final i960 registers equal where required?
- Is condition/compare state equal?
- Are local frames/procedure state equal?
- Are call/return/instruction counters equal where part of the contract?
- Is mutable Work RAM equal?
- Are other touched Model 2A regions equal?
- Are modeled hardware side effects equal?
- Did the recovery accidentally accept an unmeasured neighboring branch?
- Does a negative/control case still fail closed where it should?

A single matching final scalar is not sufficient evidence for a native block.

## Model 2A memory observer contract

The memory tracing layer exists in `src/hardware/model2a_observer.c`.

Important invariants:

- `src/hardware/model2a.c` remains the hardware behavior implementation.
- CMake renames the underlying public memory symbols to internal `*_impl`
  functions for that translation unit.
- observer wrappers call the implementation first;
- callbacks run only after `VF2_OK`;
- the callback cannot change the returned status;
- `read_u32/write_u32` are wrapped separately so common i960 `ld/st` accesses
  are observed once rather than bypassed or double-counted; and
- the observer is not serialized into `.vf2snap`.

If this architecture is changed, preserve the above semantics and run the
observer test plus sanitizer suite.

## Code conventions

- C standard: **C17**.
- Keep the core dependency-light.
- Python packages under `tools/python/requirements.txt` are analysis-only unless
  there is a compelling reason to change that boundary.
- Prefer named constants once an address/offset has stable evidence.
- Use provisional `field_xxx` names before assigning unsupported semantics.
- Keep public APIs small and passive.
- Avoid giant condition tables when a measured compact rule exists.
- Avoid broad refactors of the oracle and recovery in the same commit.
- Do not hide unsupported behavior behind defaults.
- Keep scripts reproducible: explicit seed, snapshot, mutation domain and stop
  boundary when applicable.
- Machine-readable tooling should prefer JSONL for streams.
- For very large corpora, prefer DuckDB/Parquet rather than huge in-memory JSON
  arrays.

## Repository hygiene

Do not commit:

```text
roms/
*.vf2snap
large trace JSONL/CSV files
reconstructed ROM regions
extracted game assets
out/analysis/pseudo-c generated from ROM contents
```

Compact derived metadata such as counts, hashes, branch addresses, truth-table
summaries and hand-written recovery notes are appropriate when they do not
contain proprietary game data.

## Commit/workflow policy

Unless the user explicitly asks for another workflow:

- work directly toward `master`;
- prefer one coherent commit per completed recovery/tooling slice;
- temporary staging branches are acceptable while validating a risky change,
  but squash the final result before advancing `master`;
- do not open a pull request merely as an intermediate step;
- do not mix unrelated cleanup with a recovery commit; and
- report the final commit SHA and the exact validation performed.

Never claim CI/tests passed unless you actually observed their result. If the
available environment cannot expose a check result, state that limitation.

## Common traps

### Mistaking enumeration for semantics

A 12,288-case passing matrix is excellent evidence, but a hand-written list of
12,288 accepted masks is still poor recovery if a compact measured rule exists.
Use the matrix to discover and prove the rule.

### Treating host coverage as guest coverage

Coverage of `executor.c` mostly says which interpreter cases ran. What matters
for decomp progress is the original i960 edge/address coverage.

### Naming fields too early

Repeated `fighter + 0x1a4` is evidence for a shared field. It is not, by itself,
evidence that the field is health/state/flags. Keep neutral names until behavior
supports a semantic name.

### Instrumentation changing the oracle

Tracing, taint and profiling must never alter CPU/machine semantics. Keep them
sideband and test disabled-vs-enabled equivalence.

### Overfitting one snapshot

A path matched from one state is not automatically a general recovery. Probe
neighboring conditions and keep siblings unsupported until measured.

### Pinning a register to an invented address

v0732p's fixture set `r3 = 0x0100a000` "to keep the read in range" and never
wrote to it. Every value the loop read through `r3` came back zero, and the note
promoted that to "`w3` is always zero" — with a fabricated mechanism
("`stq` stores a quadword whose high word is zero"). The disassembly said
`ld 0x0000000c(r3)[r11*16], r7` one instruction earlier, and the real entry
loads `r3 = 0x020078a8`, where all 30 words are distinct non-zero floats.

Three rules:

1. **A register pinned to a synthetic address is a fixture that cannot fail.**
   If the block reads through it, seed that window with distinct per-slot
   values, exactly as you pre-clear the destination.
2. **Read the disassembly before explaining an observation.** The mechanism
   sentence came first and the instruction listing was never consulted.
3. **Let the entry load its own registers.** Pinning `g7` alone and starting at
   the real entry is strictly better than pinning `g7`, `r8` and `r3` and
   starting inside the loop — and here it exposed that the whole block was
   already recovered.

### Re-deriving a block that is already recovered

The v0729–v0732 P2 line spent four notes analysing the `0x0d00` block as an
unidentified structure, and got a load-bearing detail wrong. `coli_2396c_body`
had been native C since v0285 and already held every constant, including the
`+0.05f` / `-0.1f` immediates and both float adds, with correct comments.

**Before characterising any block as unidentified, search the repo for an
existing recovery of the same code and read the whole function around the
boundary.** `grep` the addresses, check `docs/UNCOVERED_BRANCHES.md` history,
and read the entry's setup instructions. Four notes and one retraction were the
cost of skipping that.

A related trap: the only committed test for a recovered block may be a
*self-consistency* test. `test_coli_2396c_poly_cluster` synthesises an identity
permutation, never runs the reference executor, and zeroes the cluster — so it
could not catch an error that only appears on the real permutation, which is
precisely the error it was sitting next to. A test that does not invoke the
oracle is not a differential, whatever its assertions look like. Writing the
missing one found two further defects (`g3` dropped, `g4` uncomplemented) in a
recovery that had been "done" since v0285.

### Reading a disassembly as if it were a measurement

A disassembly line is a hypothesis. i960 three-operand forms print the
destination **last** — `cmpinco 29, r9, r9` leaves `r9 = 30` — so
`and g4, g3, g3` is `g3 &= g4`. I read it as `g4 &= g3`, rewrote two remaps
accordingly, and the new differential rejected the change immediately: the
instruction count fell from a matching 3023 to 2963, and `g7 + 0x614` went from
`0x000000fe` to `0`. The existing code had been right the whole time.

Settle operand order against something already measured before editing, and treat
a "fix" that makes a previously-matching count disagree as a wrong fix rather
than a newly discovered bug.

### Only differencing against a true control

A cell-level or counter-level diff is only evidence if the baseline differs in
exactly the variable under test. v0732f compared an INDIVIDUAL **row 2** release
against a COMMON **row 4** release, so the reported "rows 6, 11 and 13 change
with the mode" deltas were the row difference. The conclusion drawn from it —
"the INDIVIDUAL render is a different shape, not a filtered COMMON one" — was
wrong, and it nearly cost a full re-derivation of render code that already
existed. Against the correct control (same row, same input, mode the only
variable) the answer was 12 rows, all already handled.

### Differencing the rows you added proves nothing about the gate

v0732g's first version keyed the new INDIVIDUAL count rule on `coin_flags`
alone. The COMMON row-1 post-edit latch is mode-agnostic, so an INDIVIDUAL
row-1 release arrived on the same shape and the new arm claimed it as
`4062/32` — wrong by 2 instructions with **registers and memory still
matching**. A state-only differential would have passed it.

Two rules follow. First, run the differential on the *neighbouring* rows of an
admitted leg, not just the rows being admitted: that is the only thing that
proves the gate is not too wide. Second, a bug that is wrong on a counter but
right on the poststate is exactly the shape a state comparison cannot see, so
keep the instruction and call counts in the contract even when the state
matches.

### Pinning a frame total as a recovered-body count

`vf2probe` reports the whole-frame counter across the boundary; a recovered
block does not cover the whole frame. This block left a fixed 232-instruction
native tail, and that tail's *call* count differed by mode (0 under COMMON, 6
under INDIVIDUAL). Copying the measured 38 into the body claimed six calls the
original never makes. When a native count is off by a constant while
instructions match, subtract the tail rather than adjusting the base.

### Expanding the executor instead of recovering C

The reference executor should support the verified i960 instructions needed to
measure original behavior. Do not move game semantics into the interpreter to
avoid recovering them in C.

### Committing generated evidence dumps

Keep reproducible scripts and compact summaries. Do not commit the large raw
snapshot/trace corpus used to derive them.

## Definition of done

A recovery/tooling task is done when:

1. the behavior/question being addressed is explicit;
2. the evidence is reproducible;
3. the implementation is minimal and fail-closed;
4. targeted unit tests exist where practical;
5. the relevant ROM-backed differential check passes when ROM access is
   available;
6. strict build/tests pass;
7. sanitizers pass for low-level changes when applicable;
8. documentation/frontier status is updated; and
9. the final commit contains no proprietary artifacts.

## If you lose context

Do not guess what the previous agent intended. Reconstruct state from the repo:

```sh
git log --oneline -20
```

Then read:

```text
AGENTS.md
README.md
docs/UNCOVERED_BRANCHES.md
docs/PROBE_AUTOMATION_PLAN.md
docs/NATIVE_DIFFERENTIAL.md
CHANGELOG.md
```

Build the project, run the tests available in your environment, identify the
nearest explicit unsupported boundary and continue from measured evidence.

The guiding rule is simple:

> **Measure the original -> minimize the evidence -> recover the smallest C
> semantics -> prove equality -> only then expand the native frontier.**
