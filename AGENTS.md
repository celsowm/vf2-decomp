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
(an uninstrumented binary cannot exit `0xC0000135`).

**What a green run does and does not certify** (measured by planting each defect
in a scratch program built with the same flags):

| defect | caught |
|---|---|
| heap-buffer-overflow | yes |
| stack-buffer-overflow | yes |
| heap-use-after-free | yes |
| null pointer dereference | yes |
| **stack-use-after-return** | **no** - not implemented by MSVC ASan here |

So the gate is real and aborts with a non-zero exit, but a clean run says nothing
about stack-use-after-return. If you ever need that class, it has to be found
another way. See `decomp/i960/notes/sanitizer_gate_was_a_noop_v0732s.md`.

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
  descriptor, not a gameplay-state field) (114 Python test cases across
  14 `vf2_python_factory_*` ctest entries, ~13 s wall). The proven native
  dispatch boundary was re-measured at v0730
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
- the v0733a-v0733d gate-integrity and `functions.csv` slice:
  - **v0733a** proved the sanitizer gate can fail. Planted-defect map:
    heap-buffer-overflow, stack-buffer-overflow, heap-use-after-free and null
    deref are all caught; **stack-use-after-return is NOT** (MSVC limitation,
    documented as a blind spot, not as a pass).
  - **v0733b** found `block_coverage.py` ranking *container ranges* rather than
    functions (all 12 top entries were containers), and a second bug where the
    limit ran before the exclusion so the report rendered **completely empty** —
    indistinguishable from "covered everything".
  - **v0733c** corrected exactly 2 of 20 container bounds in
    `decomp/i960/functions.csv`, each only where two independent sources agreed.
    `vf2i960 function` is a **lower bound, not an extent oracle**, because it
    does not descend into callees and does not follow indirect branches (it
    measures 4 bytes for `interrupt_return_wait_exit`, whose entry is a bare
    `ret`, and 24 for `main_texture_orchestrator_call`, which branches backward
    into shared blocks below its own entry); 13 rows still carry a container
    bound and must be measured one at a time. Correcting two rows exposed 144 B
    and 28 B of code with no CSV row at all. **See the v0734a correction below —
    the instrument was never the main problem.**
  - **v0733d** built the missing `0x23524` shell differential
    (`vf2_coli_23524_live_differential`, 3 legs). The long-standing "should the
    shell publish its children's g3?" TODO is **void** — the shell's last g3
    write is `0x238a4`'s unconditional `mov 0, g3`. The `g6`-bit-0 refusal is
    now proven **load-bearing** (that leg keeps `g3=0x0000fffe`/`g4=0xffffffff`).
    Three exit fields remain unrecovered and are pinned as divergences: `g14`,
    `compare_result` and `arithmetic_control` bit 1. **`compare_result` is
    entry-state dependent and therefore unpinnable — do not "fix" it from a
    single sample.** See `decomp/i960/notes/coli_shell_contract_v0733d.md`.
  - **v0733e retracted two of those three.** `compare_result` and
    `arithmetic_control` are **exact on the warm leg**; they diverge **only on
    the live leg**. The v0733d "unpinnable" conclusion was an artifact of its own
    fixture, which stepped the reference with a hand-rolled `vf2_i960_step` loop
    while `vf2probe` and all the differential tooling use `vf2_i960_run`. **Any
    fixture that steps the reference by hand measures a different machine — drive
    it with `vf2_i960_run`.** What really remains: `g14` (path-dependent, never
    published) and the live-leg condition state (`GREATER`/`0x3f001001` where the
    native publishes the warm `EQUAL`/`0x3f001002`) — a real admitted-leg defect
    no other test caught. See
    `decomp/i960/notes/executor_harness_cc_divergence_v0733e.md`.
  - **v0733f isolated the mechanism v0733e could not.** `CMakeLists.txt:112-115`
    compiles `src/i960/executor.c` with
    `vf2_i960_step=vf2_i960_step_legacy`, so **`vf2_i960_run` — and therefore
    `vf2probe` and `vf2cycles` — calls the legacy stepper and never applies
    `arch_fix_direct_compare`, while every hand-stepping caller gets the arch
    stepper that does.** The whole divergence comes from `bbs 5, r15` at
    `0x221f0`. **Do not assume the two entry points are interchangeable, and do
    not read `compare_result` off a probe run as if it were the arch stepper's.**
    Neither path is established as arch-correct: real i960 `BBT` sets
    `ac0=<bit>, ac1=0`, which is *equal* on a clear bit, while the repo's
    `arch_fix_direct_compare` uses clear->`NONE`. Removing the macro is **not**
    done — it is a repo-wide semantic change and needs the variant-tree
    measurement first. See
    `decomp/i960/notes/executor_step_macro_asymmetry_v0733f.md`.
  - **v0733g measured it and the macro stays.** `CMakeLists.txt` now carries
    `VF2_LEGACY_STEP_IN_RUN` (default **ON**, i.e. unchanged) and prints its
    semantics at configure time. Configuring a separate tree with
    `-DVF2_LEGACY_STEP_IN_RUN=OFF` and running the suite gives **19 of 119
    failing, every one on `compare_result` only** — no branch, count or memory
    failures. The recovered `hybrid.c` was written against the legacy path, so
    **the macro is load-bearing for the current recovery and must not be
    flipped.** The i960 `BBT` convention question is a research slice, not a
    patch. B48's "assert both entry points agree" criterion is **withdrawn** —
    the test must pin the *difference*. See
    `decomp/i960/notes/executor_step_macro_measured_v0733g.md`.
- **v0734a closed Phase 1.1 and found the real bookkeeping defect.** The
    instrument was not the problem: `vf2i960 function` builds real basic blocks
    and follows **direct branch targets including backward ones**
    (`main_texture_orchestrator_call` at `0xa030` owns 17 blocks, 11 of them at
    `0x9fb0..0xa01c` *below* its entry via `b 0x9fb0`; six texture entries all
    report `end=0x4bfe0` through a shared epilogue). Its measured definition is
    **one past the highest `ret` reachable through direct branches and
    fall-through, not descending into callees and not following indirect
    branches** — so v0733c/v0733d are **upheld**, only their stated mechanism
    was imprecise. `src/analysis/cfg.c` was left **byte-identical**: an
    experimental "call is a boundary" change split blocks 3758 -> 4176 while
    moving **zero** function extents, so it was reverted as an unproven
    behaviour change.
  - **The actual defect is `functions.csv`'s `end` column being overloaded.**
    **25 of the 94 bounded rows carry a call-return continuation, not a code
    extent** (their `notes` say "returns to the main loop" / "returns to the
    interrupt dispatcher"), which puts `end < address` and makes `byte_size`
    negative. `block_coverage.py` dropped **all 25 at `load_functions()`'s
    `end <= start` with no warning** — 94 rows in, 69 out. The v0733b "empty
    report looks like full coverage" defect, one level up. The tool now names
    every dropped row on stderr and in the text report, plus a
    `--strict-ranges` gate proven to fail on a planted row.
    **Do not "repair" an inverted row by guessing an extent** — it is a
    different quantity, not corrupt data. All 25 true extents are
    ROM-measured in `function_extent_measured_v0734a.md`; three of them
    (`frame_dispatch_tick`, `player_update_gate`, `tile_controller_update`) are
    `indirect=yes` and remain **lower bounds only**. **The coverage arithmetic
    carried through v0733b-v0733g (the 267 628 B overlap, the 93 748 B gap) is
    contaminated and must be recomputed once these rows are classified.**
- **v0734b repaired them, and found a second malformation in the same file.**
    `end` is now **always** a code extent; the call-return continuation moved to
    a new **trailing** `return_to` column. Trailing is load-bearing:
    `src/analysis/symbols.c` reads this CSV by **column index**
    (`name_column = 2`), so an inserted column would have silently renamed every
    function. Migration required an independent second source per row — the
    instruction at `measured_end - 4` decoding as `ret` straight from the ROM —
    and **24 of 25 confirmed, 0 refused**; 9 also land on another row's start.
    `main_post_timer` is a **lower bound** (its highest block ends `b 0x9fb0`,
    not `ret`) and is labelled in its own `notes` rather than laundered into an
    extent, as are the three `indirect=yes` rows.
    **Five more rows carried UNESCAPED commas inside `notes`**, so every
    `DictReader` consumer silently read a truncated note and dropped the rest
    into the `None` restkey. All five are quoted now.
    Bounded rows visible to `block_coverage.py` went **69 -> 94**; inverted rows
    **25 -> 0**; `--strict-ranges` now exits 0 where it exited 1.
    The published decomp.dev number is **unchanged** (0.4456%), as v0734a's
    audit predicted. Both gates proven able to fail by planting the defect. See
    `function_extent_migrated_v0734b.md`.
- **v0734i took the Phase 1.2a conservative decisions.** One bound repaired
    under the two-source rule, eight annotated. **Repaired**: `interrupt_return_wait_exit`
    `0xd20..0x10fa4` -> `0xd20..0xd24` (4 B; `vf2i960 function` measures
    `0xd24` AND `vf2i960 disasm` fails at `0xd24`); the intervening 66,556 B
    is a real unknown gap, not part of the row (v0734h's `0x1200..0x1290`
    and `0x12bc..0x12d8` are the first two real functions inside it).
    **Shrunk**: `texture_final_status_call` `0x4bf90..0x4d25c` ->
    `0x4bf90..0x4bfe0` (80 B in 7 blocks; shared `ret` at `0x4bfdc`).
    **Annotated, kept at region bound** (5 texture cluster rows):
    `texture_status_dispatch_call`, `texture_active_prepare_call`,
    `texture_status_scan_end`, `texture_child_zero_gate_a`,
    `texture_child_zero_gate_b`. The convergence on `0x4bfe0` is a real
    CFG result (50 blocks, six entry points) — the overlap is by design.
    **Annotated, identity open** (2 rows): `camera_post_update_gate`
    (prose `0x1d984`, sweep `0x1ee34`) and `frame_shadow_verify` (prose
    "28 instructions", sweep 110 words). `src/` byte-identical.
    Published percentage unchanged at 0.4456%. See
    `phase_1_2a_conservative_decisions_v0734i.md`.
- **v0734j bounded Phase 2.6 / F4.** Status record, not a fix. F4 is
    INDIVIDUAL walks at rows 1-2 only (rows 3-5 are unreachable by walking
    in INDIVIDUAL mode: row 2's down-neighbour is row 0, and the step
    also exits INDIVIDUAL mode). Four-evidence failure modes documented:
    (1) the 4061 vs 4060 one-instruction delta between two row-2
    INDIVIDUAL samples; (2) the 38 vs 32 call-count delta;
    (3) the INDIVIDUAL render structural difference (rows 24-33 erased,
    chute section dropped, digit cells + `runs[]` filter need
    re-deriving against a reference trace); (4) the 4293 total which has
    to agree on the exact snapshot, not a cold-start. The focused slice
    needs the row-1 INDIVIDUAL PUNCH/KICK samples + row-2 ones, an
    exact-snapshot differential, and a one-conditional filter
    (`coin_flags & 1`). No `src/` change. See
    `phase_2_6_f4_status_v0734j.md`.
- **v0734k closed Phase 1.3b.** Disassembly pass on the four smaller
    gap runs that v0734h did not cover: 0x4d2c0..0x4e808 (5,448 B),
    0x6428c..0x657dc (5,456 B), 0x6cb0c..0x6dcb8 (4,524 B),
    0x658a4..0x6ca64 (29,120 B). All four are real functions, not
    padding. Each has a shared-epilogue cluster in the middle (the
    same pattern v0734a documented at 0x4bfe0 for the texture cluster)
    and a bulk of unparseable bytes at the front. `src/` byte-identical.
    See `phase_1_3b_gap_characterisation_v0734k.md`.
- **v0734l closed Phase 2.6 / F4.** Three INDIVIDUAL releases FULL MATCH
    on the current build: `indk-c` (row-1 KICK) 4293/38,
    `indp-c` (row-1 PUNCH) 4293/38, `indk2-c` (row-2 KICK) 4294/38.
    Both reference (`vf2probe --until 0x9ff8`/`0xa010`) and native
    (`vf2i960 native-resume`) legs reproduce the v0732g measurements.
    New ctest entry: `vf2_f4_individual_release` (ctest #123, 15.26 s).
    `src/` byte-identical. See
    `f4_individual_release_recovered_v0734l.md`.
- **v0734m documented Phase 2.3-2.5.** Status record, not a fix. None
    of the three are admitted beyond what v0732c/v0732g pinned.
    **2.3** (`+0x110`/`+0x114` setbits): warm leg never fires them
    (per `fa_coli_2396c_v0285.md`); live leg that fires has never been
    measured — needs fixture + memory trace. **2.4** (7-instruction
    `a5=4` delta): `preset 0 -> 4193`, `preset >= 1 -> 4186`; render
    difference is one tile write (second blank at (24,47)), so cause
    is elsewhere in the body — needs two `vf2probe --trace` runs and
    IP-precise comparison. **2.5** (edit paths 4505/4506/4509, 4625,
    4634, 4636): all six refuse at the `0xa6c0` gate by design —
    honest refusal, no silent acceptance. Needs only a ctest audit.
    `src/` byte-identical. See `phase_2_3_to_2_5_status_v0734m.md`.
- **v0735 wrote the advance plan.** Three substantial slices in
    dependency order: Phase 2.5 ctest audit → Phase 3 first
    decomposition → Phase 4 first chunk. Pure planning note; no code
    change. See `advance_plan_v0735.md`.
- **v0736 closed the Phase 2.5 ctest audit.** New ctest entry
    `vf2_phase_2_5_refused_audit` (ctest #124, 1.31 s). 7 refused paths
    still refuse (4 F2 row-3 edits + 3 INDIVIDUAL a5 patches); 2
    documented silent-admission holes (`f2-r3-c3a-e2`,
    `f2-r3-c3a-e4`). The holes are reported but not failed — closing
    them is Phase 2.5 fail-open follow-up. See v0737 and v0738 for
    the boundary characterisations.
- **v0737 characterised the Phase 3 next-target boundary.** The 4
    callees of `0x29414` are measured and named: `0x29598` (84 B,
    called twice — natural first target), `0xcf04` (184 B),
    `0x439ac` (80 B), `0x43888` (200 B, `VF2_SELECTOR2_QUEUE_ENTRY`).
    Full disassembly of `0x29598` plus a recovery recipe. No code
    change. See `fa_player_29414_callees_boundary_v0737.md`.
- **v0738 characterised the Phase 4 simulation systems boundary.**
    Post-`0x28780` frontier: physics / hitbox / damage / ring-out /
    CPU logic. Smallest is physics / idle-to-crouch transition.
    Recovery recipe in the note. No code change. See
    `phase_4_simulation_boundary_v0738.md`.
- **v0741 recovered `0x29598` in C** (the first callee of `0x29414`
    from v0737). Three paths admitted: skip (g0 bit 4 clear),
    bbc-taken (g1=0), addo-1 g1 (g1 != 15). Path D (g1 == 15,
    setbit + 3 sub-calls) REFUSED — the sub-callees `0xcf04`,
    `0x439ac`, `0x43888` are not yet recovered. New ctest entry
    `vf2_player_29598` (ctest #38, 0.03 s) PASSES. The function
    is a standalone execute hook, **not** yet wired into the
    dispatcher chain. Wiring is a separate slice. See
    `fa_player_29598_recovered_v0741.md`.
- **v0742 closed Slice 4 attempt** as a Phase 4 / crouch
    boundary status, NOT a recovery. The v0738 recipe was
    hand-wavy on the actual state byte offset and the
    post-`0x28780` corridor is a 32-iteration object/stream
    expansion loop, not a crouch transition. The actual fighter
    state byte is at `+0x01b0` (1B), and the state descriptor
    table is at `0x0200620c` (32 entries). The crouch transition
    is in some input-processing callee between `0x14288` and
    `0x180bc`; TBD via trace. Smaller alternative Phase 4
    chunk: hitbox/hurtbox at `+0x61e`/`+0x626` (state-25
    walker scaffolding already in place). See
    `phase_4_crouch_boundary_v0742.md`.
- **v0743 measured the 7-instruction a5=4 delta.** Two
    `vf2probe --trace` runs (preset=0 vs preset=1) diverge at
    step 2653 (`ip_before=0x60dbc`). The conditional is
    `r9 = r6 & 0xf; if (r9 != 1) jmp 0x60ddc`, where
    `r6 = *(uint8_t*)(*(uint32_t*)(0x614c4 + (r3 & 0xf) * 4))`.
    At preset=0 the indirect byte has lower nibble 1, so the
    7-instruction body (`stos r10, (g9)` to the row-24 chute
    slot) runs. At preset>=1 the byte has a different lower
    nibble, so the body is skipped. The 7-instruction delta
    is the chute-slot write that v0732c corrected. The
    recovered `phase17_bit7_index5` keeps the
    measured-constant rule; this note documents the
    mechanical cause. No `src/` change. See
    `phase_2_4_a5_4_divergence_v0743.md`.
- **v0744 documented the 0x29598 wiring boundary.** The v0741
    recovery is unit-tested (ctest #38 PASSES) but not yet
    active in the running code. The dispatcher chain in
    `hybrid_execute_player_post_29414` runs 0x29598 via
    `hybrid_execute_interpreted_until(0x28178, 0x14400)`,
    which uses `vf2_i960_run` with only `stop_address` /
    `max_steps` — not a per-step callback. Three candidate
    wiring approaches documented. The cleanest is a generic
    per-IP hook table checked after each `vf2_i960_step`. No
    `src/` change. See `fa_player_29598_wiring_v0744.md`.
- **v0745 recovered `0x439ac` in C** (the second callee of
    `0x29414` from v0737). A 80-byte queue dedup-append:
    read count at `0x50406a`; if >= 4, return; else search
    `0x504074[count+1..1]` for g0; if found return (idempotent);
    else write g0 to `0x504078[count]` and increment count. All
    4 paths recovered (full / match / append / mid-match).
    New ctest entry `vf2_player_439ac` (ctest #39, 0.01 s)
    PASSES. Standalone execute hook. See
    `fa_player_439ac_recovered_v0745.md`.
- **v0746 recovered `0x43888` in C** (the third callee of
    `0x29414`). A 200-byte selector2 queue entry with 6 paths
    plus a count-full negative control. Gates on
    `(0x50002c & 0xc)`, `0x500068` bit 20, and a g0 magic-value
    check (`(g0 & 0x00ff0000) == 0x009e0000`); on accept, writes
    g0 to the 16-entry ring buffer at `0x504020` indexed by
    `0x504003`, increments `0x504001` count, and pokes the
    video register `0xe80004` with 33 and 0x421. Path F
    subtracts 0x20000 from g0 before the queue write. New ctest
    entry `vf2_player_43888` (ctest #40, 0.02 s) PASSES.
    Standalone execute hook. See
    `fa_player_43888_recovered_v0746.md`.
- **v0747 recovered `0xcf04` in C** (the fourth callee of
    `0x29414`). A 184-byte post-frame IRQ handler with 2 paths
    (bit 15 of `0x500068` set vs clear). Path A reads
    `0x50005b`, mods by 11, writes to `0x50005b` and `0x500064`,
    copies `0x50a700` to `0x50a00c`. Path B reads `0x50054`,
    looks up `0x12508[r3*2]`, writes to `0x500064`, copies
    `0x50a704` to `0x50a00c`. Common tail clrbit 15 of
    `0x500068` and refuses the sub-call to `0x1fcc0`
    (`display_profile_apply`, 548 B, not yet recovered).
    **Notable finding**: the `addo 1, r3, r3` at `0xcf38` is
    **dead code** — the prologue's setbit 21 at `0x500068` is
    unconditional and the `bbs 21` at `0xcf34` re-reads after
    the setbit, so the branch is always taken and the addo
    never runs. New ctest entry `vf2_player_cf04` (ctest #41,
    0.02 s) PASSES. Standalone execute hook. See
    `fa_player_cf04_recovered_v0747.md`.
- **v0748 `0x1fcc0` boundary investigation** — 6 sub-callees named,
    5 of which became standalone recovery targets (v0749–v0754).
    See `fa_player_1fcc0_boundary_v0748.md`.
- **v0749 recovered `0x1fee4` in C** — trivial 26-iter init writing
    IEEE 754 1.0 to `0x50a0e0..0x50a144`. ctest #42, 0.02 s PASSES.
    See `fa_player_1fee4_recovered_v0749.md`.
- **v0750 recovered `0x1ff0c` in C** — display_profile_mode_constants
    (240 B, 3 paths: mode==10 / mode==6 / default). Calls v0749
    inline. ctest #43, 0.02 s PASSES. See
    `fa_player_1ff0c_recovered_v0750.md`.
- **v0751 recovered `0x1fffc` in C** — display_color_profile_apply
    (88 B, 2 paths: bit 21 of `0x500068` clear / set). 3 byte-stores
    to `0x5000e0..0x5000e2` from ROM table at `0x6eeb8+offset`;
    refuses the sub-call to `0x2c38` (color_table_rebuild, 432 B,
    11 blocks, complex — deferred). **Test bug fix**: original
    test used `0xffefffff` for "bit 21 clear" but that's actually
    bit 21 SET; correct value is `0xffdfffff`. ctest #44, 0.02 s
    PASSES. See `fa_player_1fffc_recovered_v0751.md`.
- **v0752 recovered `0x4b410` in C** — video_command_submit
    (60 B, 5 writes to `0x5502e4..0x5502f4`). ctest #45, 0.02 s
    PASSES. See `fa_player_4b410_recovered_v0752.md`.
- **v0753 recovered `0x11704` in C** — video_table_expand_128
    (64 B, 5 blocks, nested loop). Reads outer count from
    `0x78d0c`, copies 128 bytes per outer iter from `0x78d10` to
    `0x12800000` (luma RAM) byte-by-byte (ldob) with 4-byte
    destination stride (st). ctest #46, 0.01 s PASSES. See
    `fa_player_11704_recovered_v0753.md`.
- **v0754 recovered `0x2eab8` in C** — display_runtime_initialize
    (364 B, 1 block). Performs ~36 writes to `(*0x500814 + offset)`
    and 3 work-RAM stores at `0x50a160..0x168`. Sub-call to
    `0x31004` (display_transform_defaults) fully INLINED (6 writes
    to `(*0x50084c + offset)`). No refused sub-calls. ctest #47,
    0.02 s PASSES. See `fa_player_2eab8_recovered_v0754.md`.
    **Sub-callees of `0x1fcc0` — final state**: 5 of 6 recovered
    (v0749/v0750/v0751/v0752/v0753/v0754). Only `0x2c38` remains
    (deferred — disasm ambiguity at `0x2d40` `subo 1, 0, g1`).
- **v0755 per-step hook boundary investigation** — NOT yet wired.
    Design documented. 10 recovered sub-callees (v0741–v0754)
    would be activated by a per-step hook in
    `hybrid_execute_interpreted_until`. Discovered a compatibility
    issue with v0741 path D's partial-simulation pattern: setting
    `cpu->ip = 0x295e8` on refuse skips the 3 sub-calls and the
    `mov 0, g1` between entry and ret, which would diverge from
    the original i960 if the dispatcher falls back to
    interpretation. Two ways to resolve: (A) retract path D's
    partial simulation, or (B) inline the 3 sub-calls into path D.
    Recommended: (A) for cleanest per-step fit. See
    `per_step_hook_boundary_v0755.md`.
- **v0755a retracted v0741 path D's partial simulation**. Path D
    now refuses cleanly with `cpu->ip` unchanged at 0x29598 and no
    side-effects applied. ctest #38 updated to verify the clean
    refusal. This unblocks the per-step hook design. See
    `path_d_refused_cleanly_v0755a.md`.
- **v0755b implemented the per-step hook infrastructure** and
    wired 4 callees of `0x29414` (v0741/v0745/v0746/v0747).
    Added `g_callee_hooks[]` table + `hybrid_find_callee_hook` +
    `hybrid_range_has_hooks` in `src/recovered/hybrid.c`. The
    per-step loop in `hybrid_execute_interpreted_until` fires the
    hook when `cpu->ip` lands on a registered entry, replacing
    `vf2_i960_run` for ranges with at least one hook. The 0x16504
    special case is preserved. F4 INDIVIDUAL release differential
    (ctest #133) PASSES, proving the per-step loop produces the
    same final state as the legacy stepper for the 0x29414
    corridor. The other 6 sub-callee recoveries (v0749–v0754) are
    NOT yet wired — they live inside `0x1fcc0` (display_profile_apply)
    which is not in any currently-interpreted range. Wiring them
    requires recovering `0x1fcc0` (depends on `0x2c38`). See
    `per_step_hook_implemented_v0755b.md`.
- **v0755c added per-hook fire counters** and exposed
    `vf2_hybrid_get_callee_hook_counts` + `vf2_hybrid_reset_callee_hook_counts`
    + `vf2_hybrid_run_interpreted_until` (public wrapper for the
    per-step loop). `vf2i960 native-resume` now prints a
    `hook_fires:` line showing how many times each hook fired.
    **Significant finding (v0755c)**: investigation revealed
    that the per-step loop is **DORMANT in all current test
    scenarios**. The 4 wired callees of `0x29414` are wired in
    the dispatcher chain, but no test snapshot triggers the
    `hybrid_execute_player_post_29414` entry point that calls
    `hybrid_execute_interpreted_until(0x28178, 0x14400)`. The
    F4 differential's native leg starts at 0x9ff8 (from
    `out/indk-c.vf2snap`), which is INSIDE the 0x28178..0x14400
    range BUT only reached AFTER the post-29414 task has
    finished. The native resume's block chain doesn't
    re-dispatch the post-29414 task. The hook is wired but
    currently inactive. The infrastructure is in place for
    future use. Two new ctest entries added: #48
    (vf2_callee_hook_counters, PASSES — sanity check on the
    instrumentation) and #49 (vf2_callee_hook_fires_native,
    PASSES partial — reaches hook entry 0x29598 with depth=2
    via a hand-crafted fake-ROM program; the hook itself
    doesn't fire because the fake ROM has zeros at 0x29598,
    which fails the per-step loop's "step, then check hook"
    ordering before the hook check). See
    `per_step_hook_counters_v0755c.md`.
- **v0755f recovered `0x2c38` in C** — color_table_rebuild
    (432 B, 11 blocks). A 27x47 nested-loop color table fill
    into `0x54612e..0x54612e + 7614 bytes`. The recovery
    matches the EXECUTOR's interpretation of the
    `subo 1, 0, g1` instruction at `0x2d40` (the disasm-vs-
    executor ambiguity flagged in v0755). The recovery is
    **not currently validated** against the original i960
    because no differential test exercises `0x2c38` (the
    F4 differential's native leg starts at `0x9ff8`, in the
    `0x28178..0x14400` range but only reached AFTER the
    post-29414 task has finished). New ctest entry
    `vf2_player_2c38` (ctest #48, 0.03 s) PASSES. **All 6
    sub-callees of `0x1fcc0` (display_profile_apply) are now
    recovered** (v0749–v0755f). The `0x1fcc0` parent is
    recoverable by inlining all 6 sub-callees once the
    per-step hook is activated. See
    `color_table_rebuild_executor_v0755f.md`.
- **v0755g recovered `0x1fcc0` in C** — display_profile_apply
    (548 B, 15 blocks), inlining all 6 sub-callees:
      `0x1ff0c` (mode constants; inlines `0x1fee4`),
      `0x1fffc` (color profile apply; refuses `0x2c38`),
      `0x4b410` (video command submit),
      `0x2eab8` (display runtime initialize),
      `0x11704` (video table expand_128).
    The `0x2c38` refusal lives inside the inlined `0x1fffc`
    (matching v0751's existing refusal pattern), so this
    function returns `VF2_ERROR_UNSUPPORTED` with
    `cpu->ip == 0x20050`. Three design notes:
    (a) Combo detection uses explicit `goto combo_set` to
        mirror the two distinct i960 fall-through structures
        (the (2,1) combo jumps via `cmpobe 1`; the (1,2)
        combo falls through from block `0x1fce8`).
    (b) `0x018021ee` is an unmapped VDP1 control register;
        the recovery reads the ROM value, attempts the write,
        and treats `VF2_ERROR_OUT_OF_BOUNDS` as expected (any
        other error aborts). The per-step hook's interpretation
        fallback would fail similarly.
    (c) The test attaches a fake 0x80000-byte `main_rom` so
        ROM reads succeed end-to-end.
    New ctest entry `vf2_player_1fcc0` (#48 by the recovery
    count, 0.02 s) PASSES — exercises 6 paths (combo 2+1,
    combo 1+2, bit21+bit20 set, mode=10 + 0x4c=2, default,
    bit21-only default). See
    `fa_player_1fcc0_recovered_v0755g.md`.
- **v0755h first observation of the `0x2c38` saturation ambiguity**
    — `subo_saturation_2d40_v0755h.md`. Forces saturating inputs
    on the inner loop's first row's last inner entry and verifies
    the recovery's output matches the EXECUTOR semantics
    (`g1 = 0xFFFFFFFF`, then `255 * 0xFFFFFFFF >> 7 & 0xFFFF =
    0xFFFE`). The disasm convention would yield `0x0001`. This
    test PASSES — it documents the current behavior as a
    regression guard. **It does NOT resolve the ambiguity.**
    Resolution requires a live ROM-backed differential that
    exercises `0x2c38`; no such snapshot exists in the current
    corpus (the per-step hook remains DORMANT per v0755c). New
    ctest entry `vf2_subo_saturation_2d40` (#49, 0.01 s) PASSES.
- **v0756 recovered `0x323fc` in C** — `post_cf04_combat_state_clear`
    (164 B, 7 blocks). Calls `0xcf04` (already recovered v0747),
    clrbit 19 of `0x500068`, then dispatches on bit 0 of `*(g13)`:
    bit 0 set path toggles `0x500056` and setbits 1/3 of `*(g13)`
    + stores `0x324a0` to `(g13 + 0xc)`; bit 0 clear path is a
    no-op ret. The `0xcf04` refusal of the `0x1fcc0` sub-call is
    discarded by the inline (cf04's body work has been applied;
    only the trailing `clrbit 21` is deferred). g13 reads come
    from `cpu->registers[29]` directly. New ctest entry
    `vf2_player_323fc` (#50, 0.02 s) PASSES — 4 paths (bit-0 +
    0x500056 toggle, path B, path C). See
    `fa_player_323fc_recovered_v0756.md`. Sibling `0x32284` shares
    the trailing body but clrsbit `18` instead of `19`.
    - **v0756b recovered `0x32284`** — `post_cf04_combat_state
    _clear_v18`, the clrbit-18 sibling of `0x323fc`. Same body,
    different bit parameter. New ctest entry `vf2_player_32284`
    (#51, 0.01 s) PASSES.
    - **v0757 recovered `0x4421c`** — `post_init_floats_helper`
    (76 B, 1 block). Tiny fixed-constant initializer that runs
    after the 1.0 float init (`0x1fee4`). Writes 5 fixed constants
    to known offsets. New ctest entry `vf2_player_4421c` (#52,
    0.01 s) PASSES.
    - **v0758 recovered `0x7ef0`** — `rom_to_wram_triple_copy`
    (36 B, 1 block). Copies 24 bytes from ROM at `0x7f64` to
    work RAM at `0x501400` via two `ldt`/`stt` pairs. The test
    attaches a fake `main_rom` covering the source reads. New
    ctest entry `vf2_player_7ef0` (#53, 0.01 s) PASSES.
    - **v0759 recovered `0xa154`** — `zero_loop_43_dwords`
    (36 B, 3 blocks). A `cmpdeco`-driven loop that clears 43
    dwords (172 bytes) of work RAM starting at `0x501800`.
    New ctest entry `vf2_player_a154` (#54, 0.01 s) PASSES.
    - **v0760 recovered `0x4ad40`** — `zero_workram_helpers`
    (52 B, 1 block). Clears 4 work-RAM fields: short zeros at
    `0x5502a8`, `0x5502b0`, `0x5502b8` and word zero at `0x546000`.
    New ctest entry `vf2_player_4ad40` (#55, 0.01 s) PASSES.
    - **v0761 was committed then retracted.** The 0x1fcc0
    (`display_profile_apply`) hook wiring in `g_callee_hooks[]`
    broke F4: the F4 native path does reach 0x1fcc0, the recovery
    refuses at 0x20050, and the per-step loop's UNSUPPORTED path
    then steps one more instruction (the `ret`), popping the wrong
    frame and entering an infinite loop (>600 s ctest timeout).
    Reverted via `git reset --hard d9f7ba0a && git push
    --force-with-lease origin master`. Diagnosis in
    `per_step_hook_1fcc0_wiring_retracted_v0761.md`. Three next
    options documented in that note: rewrite the 0x1fcc0 recovery
    to return VF2_OK; harden the per-step loop's UNSUPPORTED
    handling; or leave 0x1fcc0 unwired until one of the above is
    done. `master` is currently at the v0760 AGENTS.md sync
    (`d9f7ba0a`); this entry is the only record that v0761
    ever existed.

The most recent tooling layer is intentionally **above** the validated executor.
It accelerates evidence gathering; it does not replace the oracle.

## Completion plan

The measured state and the full remaining plan — ordered by **dependency, not
impact** — live in `decomp/i960/notes/completion_plan_v0734.md`. Read it before
starting new work. Phase 0 (oracle integrity: the `vf2_i960_run` vs hand-stepped
condition-state divergence, B45) gates the credibility of everything after it;
Phase 1 is bookkeeping; Phase 2 is the named fail-closed admissions; Phase 3-4
are the actual bulk (`fa_player` downstream, then the simulation systems, which
are essentially unstarted); Phase 5 is semantic naming and is evidence-gated.

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
