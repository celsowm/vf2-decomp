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
- `docs/ORIGINAL_SYMBOLS.md` — the 301 shipped Sega symbol names, and which
  provisional names in this repository they contradict.
- `decomp/i960/notes/` — address-level evidence.
- `CHANGELOG.md` — useful context for why a boundary exists.

Do not trust a status sentence in this file over newer measured evidence. If the
repository has advanced, update this handoff as part of the same work.

## Windows environment

If the agent is running on **Windows, use WSL2** for development work on this
repository.

Run the build, CMake/Ninja/CTest commands, Python recovery tooling, shell scripts
and ROM-backed differential workflows from inside WSL2. Prefer keeping the
working tree inside the Linux filesystem (for example under `~/src/`) instead of
building from `/mnt/c/...`, especially for large builds and trace-heavy analysis.

Do not create a separate native-Windows PowerShell/MSVC workflow unless the user
explicitly asks for Windows-native support. The canonical agent workflow on
Windows is WSL2 so behavior stays aligned with Linux CI and the documented shell
commands.

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
  -DVF2_BUILD_TESTS=ON \
  -DVF2_WARNINGS_AS_ERRORS=ON \
  -DVF2_ENABLE_SANITIZERS=ON \
  -DVF2_ROM_DIR=/path/to/vf2
cmake --build build-san --parallel
ctest --test-dir build-san --output-on-failure
```

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
  accesses.

The most recent tooling layer is intentionally **above** the validated executor.
It accelerates evidence gathering; it does not replace the oracle.

## Important current gameplay frontiers

Always confirm these against `docs/UNCOVERED_BRANCHES.md` before coding.

### `fa_game_info` around `0x00018644`

A large state-4/state-8 matrix is already recovered. State-8 bit 6 is exact for
the complete negative-threshold matrix, and substantial positive-threshold
coverage exists. Some **positive bit-6 compositions remain explicitly
unsupported**.

Do not add another giant hand-written mask table unless the measured behavior
really requires one. Prefer:

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
leverage.

### 1. Extend `frontier.py`

`tools/python/frontier.py` now provides the initial queryable frontier from
guest coverage and native/unsupported boundaries. The useful unit is a guest
address/edge, not host C coverage. It ingests corpus manifests, sweep JSONL
and trace JSONL, ranks candidates by measured witnesses, reproducible
snapshot availability, unsupported-final counts and recovered-range
attribution from `decomp/i960/functions.csv`, and supports
`--exclude-recovered` to show only the working frontier.

Remaining extensions, in rough value order:

- read/write activity around the boundary (correlate `--memory-trace`
  access clusters with candidate edges);
- call target attribution when the edge source is a `call` instruction;
- DuckDB-backed persistence when corpus volume outgrows the streaming
  aggregator; and
- Parquet export for very large trace corpora.

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
- Before naming a function, check `decomp/i960/original_symbols.csv`. If the
  shipped table names that address, use its name. Do not invent a name over a
  known one, and do not carry a provisional name that the table contradicts.
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

### Reasoning from a provisional function name

Every function name in `decomp/i960/{symbols,functions}.csv` that predates
`decomp/i960/original_symbols.csv` was invented, and where the shipped table
overlaps them it disagrees with all 34. `frame_geometry_gate` is `test_sw_chk`,
`frame_counter_advance` is `variable_diff_calc`, `texture_header_decode` is
`unpack_lod_data`. Treat a provisional name as a label, never as an argument
about what a path does. `docs/ORIGINAL_SYMBOLS.md` lists the contradictions.

### Instrumentation changing the oracle

Tracing, taint and profiling must never alter CPU/machine semantics. Keep them
sideband and test disabled-vs-enabled equivalence.

### Overfitting one snapshot

A path matched from one state is not automatically a general recovery. Probe
neighboring conditions and keep siblings unsupported until measured.

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
