# i960 analysis frontend

## Design rule

The decoder returns structured instructions. Analysis code must never recover
operands, targets or instruction classes by parsing formatted assembly text.

## Pipeline

```text
ROM region
  -> structured decoder
  -> reachable instruction discovery
  -> leaders and non-overlapping basic blocks
  -> direct call graph
  -> abstract register interpretation
  -> indirect target and boundary candidates
  -> task-descriptor roots and names
  -> symbol overlays
  -> evidence databases and C-like pseudocode
```

## Abstract values

The static data-flow pass introduced in v0.0.3 tracks:

- unknown values;
- integer/address constants;
- stack-relative offsets;
- incoming argument registers `g0` through `g7`;
- indexed 32-bit table lookups.

Values merge conservatively at CFG joins. Conflicting values become unknown;
the analyzer does not select a convenient branch value.

## Calling-convention heuristics

The pass records:

- reads from `g0` through `g7` before a local definition as candidate arguments;
- writes to `g0` observed before return as a candidate return value;
- use of `sp` and `fp` in memory operands;
- a conservative aligned stack-frame estimate;
- leaf/non-leaf status;
- `bx (g14)` as the standard i960 return idiom used by VF2.

These fields are evidence hints, not recovered C types.

## Indirect flow

An indirect branch or call is resolved only when abstract interpretation proves
one of these forms:

- a constant effective target;
- a register loaded from an indexed 32-bit pointer table whose entries decode as
  valid aligned i960 targets.

Each result records a confidence score and source table. Unresolved transfers
remain explicitly reported.

## Boundary candidates

The analyzer reports, but does not silently rewrite, candidates produced by:

- a known function entry appearing inside another candidate's blocks;
- a terminal unconditional branch to another known function entry.

These are written to `function-splits.csv` for human review.

## Symbol overlays

`decomp/i960/functions.csv`, `symbols.csv` and `known_entries.csv` provide stable
human-assigned names. Names are sanitized as C identifiers and applied only to
matching discovered addresses. Set `VF2_SYMBOL_DIR` to use another overlay.

`decomp/i960/original_symbols.csv` provides 301 *original* Sega symbol names
recovered from the symbol table shipped inside a later Model 2 port DLL. It is
applied last and therefore wins over the three provisional overlays, because a
shipped name is evidence and a repository name is a hypothesis. See
`docs/ORIGINAL_SYMBOLS.md` for provenance, the regeneration command and the list
of provisional names it contradicts.

## Task-derived roots

The analyzer structurally locates the 29-record `fa_*` descriptor table and adds
each unique task entry point as a conservative analysis root. Shared entries are
canonicalized (`task_rob`, `task_object`, `task_selector`, `task_osage`) while
individual entries retain evidence-backed names such as `task_camera` and
`task_sound`. Task reports are written as `tasks.csv`, `tasks.json` and
`tasks.dot`.

## Generated evidence

- `i960.asm`: formatted instructions classified as code;
- `functions.csv`: functions, frame/ABI hints and indirect-flow counts;
- `function-splits.csv`: overlap and tail-branch candidates;
- `xrefs.csv`: control-flow and memory references;
- `values.csv`: propagated values after instructions;
- `indirect-targets.csv`: resolved constant/table targets;
- `stack-frames.csv`: per-function frame and ABI summary;
- `image-map.csv`: contiguous classification ranges;
- `strings.csv`: strings and resolved reference counts;
- `callgraph.dot`: named call graph;
- `cfg/*.dot`: per-function CFGs;
- `pseudo-c/*.c`: generated non-matching C-like output;
- `tasks.csv`, `tasks.json`, `tasks.dot`: recovered scheduler descriptors;
- `report.json`: measured totals.

Names in the above come from the overlays, so `i960.asm`, `callgraph.dot`,
`cfg/*.dot` and `pseudo-c/*.c` now carry the shipped symbol names wherever the
table has one.

## Known limitations

- register analysis is intraprocedural;
- memory aliasing is not modeled;
- function arguments are register-mask heuristics, not typed signatures;
- table bounds are inferred conservatively from valid entries;
- computed continuations may require caller-state propagation;
- generated pseudocode is not guaranteed to compile or preserve semantics;
- dynamic execution is deliberately separate from this static pass; v0.0.22 includes startup, task-registry and timer/scheduler execution support in `src/i960/executor.c`, plus bounded task, camera and first-dispatch scheduler recoveries under `src/recovered/`. The native second-dispatch validator executes 4,623 recovered first-sweep instructions in C, preserves all task contexts through the frame bridge and reaches the second scheduler traversal without an intermediate restore.
