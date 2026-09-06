# vf2-decomp

Clean-room, non-matching C17 decompilation research project for **Virtua Fighter 2 Version 2.1** on Sega Model 2A.

The goal is to recover the original game/runtime behavior into portable, readable C while continuously validating the recovered implementation against the original Intel i960 program.

> This repository contains **no ROMs** and is **not yet a complete playable port**.

## Project status

The project already contains a substantial recovered native runtime, ROM validation/reconstruction tools, Intel i960 analysis tooling, a bounded Model 2A hardware model, snapshot/resume support and strict differential validation between recovered C and the original program.

Current work is focused on expanding recovered gameplay/runtime state coverage while preserving exact CPU, procedure-count and mutable-memory behavior for accepted paths. Unsupported or unverified branches remain explicit instead of being approximated.

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
tests/                   ROM-independent and optional ROM-backed tests
```

## Documentation

Start here:

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — project architecture.
- [`docs/DECOMP_GUIDE.md`](docs/DECOMP_GUIDE.md) — recovery workflow and conventions.
- [`docs/I960_ANALYSIS.md`](docs/I960_ANALYSIS.md) — i960 analysis tooling.
- [`docs/EXECUTION.md`](docs/EXECUTION.md) — execution model.
- [`docs/NATIVE_RUNTIME.md`](docs/NATIVE_RUNTIME.md) — recovered runtime design.
- [`docs/NATIVE_DIFFERENTIAL.md`](docs/NATIVE_DIFFERENTIAL.md) — differential-validation contract.
- [`docs/FIRST_DISPATCH_TASKS.md`](docs/FIRST_DISPATCH_TASKS.md) — task/scheduler recovery notes.
- [`docs/UNCOVERED_BRANCHES.md`](docs/UNCOVERED_BRANCHES.md) — known remaining recovery boundaries.
- [`docs/ORIGINAL_SYMBOLS.md`](docs/ORIGINAL_SYMBOLS.md) — the 301 original Sega i960 symbol names and the provisional names they replace.
- [`CHANGELOG.md`](CHANGELOG.md) — chronological project progress.

Fine-grained address-level evidence lives under `decomp/i960/notes/` and in focused recovery documents under `docs/`.

## Scope

This project is not currently claiming:

- complete decompilation of Virtua Fighter 2;
- a production-ready or fully playable replacement executable;
- complete Sega Model 2/TGP emulation;
- full SCSP FM/DSP audio behavior; or
- coverage of every gameplay state and branch.

Those areas remain incremental recovery targets.

## Legal

The repository contains no Sega game data. Users must provide their own legally obtained ROM files for ROM-backed analysis and validation.

See [`docs/LEGAL.md`](docs/LEGAL.md) and [`THIRD_PARTY.md`](THIRD_PARTY.md).
