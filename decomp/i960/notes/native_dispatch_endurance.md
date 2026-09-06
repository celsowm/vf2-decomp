# Native dispatch endurance observations

ROM-backed measurements on 2026-09-06 using GCC `vf2i960` artifacts from the
current `master` recovery corridor.

No ROM, snapshot, trace, or other proprietary artifact is stored here. The
measurements are reproduced with a locally supplied supported ROM set:

```sh
vf2i960 native-nth-dispatch /path/to/vf2 <dispatch>
```

## Confirmed corridor

The recovered runtime and the reference i960 executor match complete final CPU
and modeled memory state through **dispatch 60**. Every sampled target returned
success and reported `Final CPU and memory state: MATCH`.

Measured targets include:

| Dispatch | Repeated-cycle blocks | Repeated-cycle instructions | Result |
| ---: | ---: | ---: | --- |
| 10 | checkpoint | checkpoint | MATCH |
| 11 | 37 | 2166 | MATCH |
| 12 | 37 | 2169 | MATCH |
| 13 | 37 | 2166 | MATCH |
| 14 | 37 | 2169 | MATCH |
| 15 | 37 | 2166 | MATCH |
| 16 | 37 | 2169 | MATCH |
| 17 | 37 | 2166 | MATCH |
| 18 | 37 | 2169 | MATCH |
| 19 | 37 | 2166 | MATCH |
| 20 | 37 | 2169 | MATCH |
| 21 | 37 | 2166 | MATCH |
| 22 | 37 | 2169 | MATCH |
| 23 | 37 | 2166 | MATCH |
| 24 | 37 | 2169 | MATCH |
| 30 | 37 | 2169 | MATCH |
| 31 | 37 | 2166 | MATCH |
| 32 | 37 | 2169 | MATCH |
| 33 | 37 | 2166 | MATCH |
| 34 | 37 | 2163 | MATCH |
| 35 | 37 | 1563 | MATCH |
| 37 | 37 | 1561 | MATCH |
| 38 | 37 | 1566 | MATCH |
| 39 | 37 | 1563 | MATCH |
| 40 | 37 | 1566 | MATCH |
| 60 | 37 | 1566 | MATCH |

All listed dispatches enter `fa_game_info` at `0x0001645c` with registry
`0x00515200`.

## Observed regime transition

The large repeated-cycle instruction-count transition occurs between dispatch
34 and 35:

```text
dispatch 34: 2163 instructions, MATCH
dispatch 35: 1563 instructions, MATCH
```

Later measured cycles vary among 1561, 1563, and 1566 instructions while still
matching the reference exactly. This is evidence of a real state-dependent path
change inside the already recovered corridor, not an unsupported boundary.

ROM-backed checkpoints on both sides of the transition reduce the full snapshot
delta to only 28 bytes. The relevant work-RAM phase state changes from:

```text
dispatch 34:
  [0x00501004] = 0x00910000
  [0x0050100c] = 2

dispatch 35:
  [0x00501004] = 0x00918000
  [0x0050100c] = 3
```

The word at `0x00501004` has only three static xrefs in the ROM-backed image:
writers at `0x00000f4c` and `0x00002f48`, and a reader at `0x00002edc`. The
`0x00002edc..0x00002f58` helper advances the byte index at `0x0050100c`, loads
the next word from the table at `0x00007a00`, stores it to `0x00501004`, and
mirrors it into the geometry-facing state rooted at `g10`.

The geometry snapshot changes at the same boundary. Therefore the roughly
600-instruction drop is currently best treated as a **geometry buffer/phase
rotation**, not evidence of a new gameplay branch. Future analysis should trace
that rotation before assigning stronger semantics.

## Continuation checkpoint accounting

The endurance investigation exposed and fixed a tooling bug in
`native-continue-dispatch`. Repeated native cycles add two scheduler entries per
logical dispatch, but the old checkpoint restore path reconstructed the dispatch
number as `scheduler_entries + 2`. A snapshot written at logical dispatch 34 was
therefore incorrectly reported as dispatch 58.

The corrected mapping is:

```text
logical dispatch = scheduler_entries / 2 + 6
```

for the validated continuation checkpoint family, with odd or structurally
invalid scheduler-entry counts rejected fail-closed. After the fix, a
ROM-backed dispatch-34 snapshot restores as dispatch 34 and continues to dispatch
35 with a complete CPU/memory `MATCH` and 1563 recovered instructions.

## Implication

The older handoff statement that the accepted repeated corridor only reaches the
seventh dispatch understates current measured coverage. The strict native
runtime survives substantially farther without interpreter fallback in this
observed scenario. This does not imply general gameplay completeness: alternate
inputs, task schedules, fighter states, camera modes, and other unmeasured
branches remain explicit boundaries.
