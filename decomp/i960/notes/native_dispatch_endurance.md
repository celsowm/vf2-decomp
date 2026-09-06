# Native dispatch endurance observations

ROM-backed measurements on 2026-09-06 using the GCC `vf2i960` artifact built
from `d429290041b33531f70d6e803f47c07cc497a979`.

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

The transition should be treated as a useful future exploration anchor: compare
state and edge coverage at dispatch 34 versus 35 before assigning semantics to
the changed path.

## Implication

The older handoff statement that the accepted repeated corridor only reaches the
seventh dispatch understates current measured coverage. The strict native
runtime survives substantially farther without interpreter fallback in this
observed scenario. This does not imply general gameplay completeness: alternate
inputs, task schedules, fighter states, camera modes, and other unmeasured
branches remain explicit boundaries.
