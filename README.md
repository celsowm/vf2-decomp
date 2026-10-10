# vf2-decomp

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/L4L11VB7JN)

Clean-room, non-matching C17 decompilation research project for **Virtua Fighter 2 Version 2.1** on Sega Model 2A.

The goal is to recover the original game/runtime behavior into portable, readable C while continuously validating the recovered implementation against the original Intel i960 program.

> This repository contains **no ROMs** and is **not yet a complete playable port**.

## Project status

As of v0.1.4, the project contains a substantial recovered native runtime,
ROM validation/reconstruction tools, Intel i960 analysis tooling, a bounded
Model 2A hardware model, snapshot/resume support and strict differential
validation between recovered C and the original program.

The validated native corridor now preserves exact CPU, condition-code,
procedure-count and mutable-memory state through the repeated scheduler and
gameplay dispatches, with measured `native-nth-dispatch` coverage through
dispatch 40 and CTest pins at dispatches 11 and 12 (the strict sixth-dispatch
base ends at the tenth `fa_game_info` entry, so dispatches 7-10 are covered
per-block inside the sixth command and `native-nth-dispatch 11` proves one
further 37-block / 2,166-instruction cycle exact). The strict post-scheduler
corridor contains 1,270,824 recovered instructions and zero interpreted
instructions.

Recent recovery work has completed the measured `fa_coli` bilateral
`field_0821` scan matrix: every composition except 6/6 is native under the
additive per-side rule `9520 + delta(F0) + delta(F1)` with full live-state
equality through `0x10dcc`, while bilateral 6/6 stays pinned fail-closed
pending the 20-step scan-6 ordering-fail child tail. The `fa_player` `0x19ef8`
49-value mask family is audited and continuously proven, and the
model2recomp-guided interrupt acknowledge at `0x0000d30` is recovered. These
are accepted only for the measured combinations; neighboring or unverified
branches still return `VF2_ERROR_UNSUPPORTED`. The TEST MENU / COIN
ASSIGNMENT corridor (rows 1-4 in COMMON and INDIVIDUAL modes) is recovered
natively, and the F4 INDIVIDUAL post-edit release is pinned FULL MATCH
(4293/38, 4293/38, 4294/38) on the reference and native legs. 14 sibling
player functions (0x29598, 0x439ac, 0x43888, 0xcf04, 0x1fcc0 with the rest
in its call chain, 0x1fee4, 0x1ff0c, 0x1fffc, 0x4b410, 0x11704, 0x2eab8,
0x323fc, 0x32284, 0x4421c, 0x7ef0, 0xa154, 0x4ad40) are unit-tested. See
[`docs/UNCOVERED_BRANCHES.md`](docs/UNCOVERED_BRANCHES.md) for the current
frontier (v0760).

The project is still a clean-room recovery and validation effort, not a
complete playable port. Character/arena selection, the complete match state
machine, fighter physics and combat, ring-out behavior, CPU decision logic,
and substantial geometry, audio and hardware behavior remain open. The
Model 2A boundary includes the measured common bus behavior for one-shot
25 MHz timers/IRQ requests, video frame status, render-mode control, TGP
program-upload accounting. Full TGP firmware execution,
polygon rasterization, tile/video timing and SCSP FM/DSP behavior remain
explicitly outside that bounded model.

### Progress toward a playable Virtua Fighter 2 port

These bars are a qualitative view of progress toward running the **complete game natively**, not a percentage of decompiled code or recovered instructions.

```text
ROM / boot              ██████████  very advanced
Model 2A hardware       ████████░░  bounded bus + TGP/SCSP/framebuffer services
scheduler / runtime     ██████████  validated native corridor
input                    ████████░░  game-facing input path; platform adapter open
camera                   ████████░░
HUD / game_disp          █████████░  TEST / COIN ASSIGNMENT / F2-F4 post-edit menu corridors
fighter / game logic     ███████░░░  post-29414 player corridor; 14 sibling function recoveries; F4 INDIVIDUAL post-edit FULL MATCH
geometry / rendering     ███████░░░  TGP/object paths recovered; rasterizer open
audio                    █████░░░░░  68000/SCSP/PCM path partly recovered; FM/DSP open
complete game flow       █████░░░░░  boot/menu/phase corridors; F2/F3/F4 post-edit; match flow open
fully playable match     ██░░░░░░░░  combat/physics/AI still open
```

For detailed development history, recovered branches and release-by-release progress, see [`CHANGELOG.md`](CHANGELOG.md).

For known remaining boundaries, see [`docs/UNCOVERED_BRANCHES.md`](docs/UNCOVERED_BRANCHES.md).

## Principles

- **Clean-room recovery:** the repository contains reconstructed behavior and analysis metadata, not Sega ROM data.
- **Fail closed:** unknown branches return `VF2_ERROR_UNSUPPORTED` instead of silently inventing behavior.
- **Differential validation:** recovered paths are compared against the reference i960 execution for CPU state, call frames, counters and mutable memory.
- **Portable C:** recovered game/runtime logic lives in C17 rather than being permanently tied to the interpreter.
- **Evidence-driven recovery:** addresses, instruction counts and branch behavior are documented from controlled observations.

## Build

```sh
cmake -S . -B build \
  -DVF2_BUILD_TESTS=ON \
  -DVF2_WARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

To enable ROM-backed differential tests, point CMake at a legally obtained supported ROM directory:

```sh
cmake -S . -B build \
  -DVF2_BUILD_TESTS=ON \
  -DVF2_WARNINGS_AS_ERRORS=ON \
  -DVF2_ROM_DIR=/path/to/vf2
cmake --build build
ctest --test-dir build --output-on-failure
```

Sanitizer build:

```sh
cmake -S . -B build-san \
  -DVF2_BUILD_TESTS=ON \
  -DVF2_WARNINGS_AS_ERRORS=ON \
  -DVF2_ENABLE_SANITIZERS=ON \
  -DVF2_ROM_DIR=/path/to/vf2
cmake --build build-san
ctest --test-dir build-san --output-on-failure
```

On native Windows (MSVC, the canonical agent workflow), build and test
from the repository root with:

```powershell
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
```

and adapt binary paths to the `build\Debug\...` output layout (for example
`build\Debug\vf2probe.exe`).

## Main tools

### `vf2rom`

Validate and reconstruct the supported ROM set:

```sh
build/vf2rom verify /path/to/vf2
build/vf2rom info /path/to/vf2
build/vf2rom extract /path/to/vf2 out/regions
```

### `vf2i960`

Analyze the main Intel i960 program:

```sh
build/vf2i960 disasm /path/to/vf2 0x0001d320 64
build/vf2i960 function /path/to/vf2 0x0001d320
build/vf2i960 analyze /path/to/vf2 out/analysis
```

Run recovered/differential execution milestones:

```sh
build/vf2i960 compare-boot /path/to/vf2
build/vf2i960 compare-init /path/to/vf2
build/vf2i960 scheduler-dispatch /path/to/vf2
build/vf2i960 native-first-dispatch /path/to/vf2
build/vf2i960 native-second-dispatch /path/to/vf2
build/vf2i960 native-third-dispatch /path/to/vf2
build/vf2i960 native-fourth-dispatch /path/to/vf2
build/vf2i960 native-fifth-dispatch /path/to/vf2
build/vf2i960 native-sixth-dispatch /path/to/vf2
build/vf2i960 native-nth-dispatch /path/to/vf2 11
build/vf2i960 native-nth-dispatch /path/to/vf2 12
```

Use `vf2i960 --help` for the full command set.

### `vf2cycles`

Resume a proven snapshot and run repeated recovered/reference cycles:

```sh
build/vf2cycles \
  --rom-dir /path/to/vf2 \
  --snapshot fifth-dispatch.vf2snap \
  --cycles 10 \
  --min-blocks 1 \
  --max-blocks 16384
```

A strict run stops on the first unsupported native block, reference failure or state mismatch. `--boundary-probe` is available for longer scouting runs where complete equality is checked at cycle boundaries rather than after every recovered block.

### `vf2probe`

Reproducible machine-readable experiments: restore a snapshot, patch
registers/memory, stop at a guest address, and emit instruction and Model 2A
memory-access traces:

```sh
build/vf2probe \
  --rom-dir /path/to/vf2 \
  --snapshot checkpoint.vf2snap \
  --until 0x000164c4 \
  --trace \
  --memory-trace
```

### `vf2recover`

Human-readable recovery reports around a checkpoint, for analyst inspection
rather than bulk machine processing.

### `vf2m68k`

Inspect the 68000 audio program:

```sh
build/vf2m68k info /path/to/vf2
build/vf2m68k disasm /path/to/vf2 0x100 64
```

## Differential validation

The reference executor and recovered C runtime can be advanced from the same snapshot and compared at controlled boundaries.

Accepted native paths are expected to reproduce, as applicable:

- Intel i960 registers and condition state;
- local procedure frames;
- instruction/call/return counters;
- scheduler/task state;
- work RAM and other mutable Model 2A regions;
- device-visible state modeled by the project; and
- runtime sidecar state used for resumable frame execution.

The interpreter remains a validation oracle and exploration tool. The long-term target is recovered native C, not an interpreter-dependent port.

See [`docs/NATIVE_DIFFERENTIAL.md`](docs/NATIVE_DIFFERENTIAL.md), [`docs/NATIVE_RUNTIME.md`](docs/NATIVE_RUNTIME.md) and [`docs/EXECUTION.md`](docs/EXECUTION.md).

## Repository layout

```text
config/                 supported ROM manifest and configuration
decomp/i960/            symbols, task descriptors and recovery evidence
docs/                   architecture, execution, status and recovery docs
include/vf2/            public C APIs
src/analysis/           CFG, xrefs, semantics and pseudocode generation
src/i960/               i960 decoder, reference executor and snapshots
src/hardware/           bounded Sega Model 2A memory/device model
src/recovered/          accepted semantic C recoveries
tools/vf2rom/           ROM validation and region reconstruction
tools/vf2i960/          analysis and differential-validation CLI
tools/vf2probe/         machine-readable controlled experiments
tools/vf2recover/       human-readable recovery reports
tools/vf2cycles/        repeated recovered/reference cycle runner
tools/python/           analysis-only sweep/explore/trace/frontier tooling
tests/                   ROM-independent and optional ROM-backed tests
```

The automated probing workflow (`make_game_info_probe_scenario.py`,
`check_scenario.py`, `sweep_state.py`, `infer_rules.py`, `explore_state.py`,
`minimize_case.py`, `trace_case.py`, `infer_structs.py`, `frontier.py`) is
documented in [`docs/PROBE_AUTOMATION_PLAN.md`](docs/PROBE_AUTOMATION_PLAN.md).

## Documentation

Start here:

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — project architecture.
- [`docs/DECOMP_GUIDE.md`](docs/DECOMP_GUIDE.md) — recovery workflow and conventions.
- [`docs/I960_ANALYSIS.md`](docs/I960_ANALYSIS.md) — i960 analysis tooling.
- [`docs/EXECUTION.md`](docs/EXECUTION.md) — execution model.
- [`docs/NATIVE_RUNTIME.md`](docs/NATIVE_RUNTIME.md) — recovered runtime design.
- [`docs/NATIVE_DIFFERENTIAL.md`](docs/NATIVE_DIFFERENTIAL.md) — differential-validation contract.
- [`docs/FIRST_DISPATCH_TASKS.md`](docs/FIRST_DISPATCH_TASKS.md) — task/scheduler recovery notes.
- [`docs/PROBE_AUTOMATION_PLAN.md`](docs/PROBE_AUTOMATION_PLAN.md) — automated probing/exploration workflow.
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — milestone roadmap including first playable scope.
- [`docs/STATUS.md`](docs/STATUS.md) — component-level status table.
- [`docs/UNCOVERED_BRANCHES.md`](docs/UNCOVERED_BRANCHES.md) — known remaining recovery boundaries.
- [`docs/ORIGINAL_SYMBOLS.md`](docs/ORIGINAL_SYMBOLS.md) — the 301 original Sega i960 symbol names and the provisional names they replace.
- [`CHANGELOG.md`](CHANGELOG.md) — chronological project progress.

Fine-grained address-level evidence lives under `decomp/i960/notes/` and in focused recovery documents under `docs/`.

## Scope

This project is not currently claiming:

- complete decompilation of Virtua Fighter 2;
- a production-ready or fully playable replacement executable;
- complete Sega Model 2/TGP emulation (the current model is a bounded common
  Model 2A bus/device boundary);
- full SCSP FM/DSP audio behavior; or
- coverage of every gameplay state and branch.

Those areas remain incremental recovery targets.

## Legal

The repository contains no Sega game data. Users must provide their own legally obtained ROM files for ROM-backed analysis and validation.

See [`docs/LEGAL.md`](docs/LEGAL.md) and [`THIRD_PARTY.md`](THIRD_PARTY.md).
