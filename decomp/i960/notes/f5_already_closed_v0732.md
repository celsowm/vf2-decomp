# v0732e: F5 is already closed - the v0730 premise is false

**Evidence only. No behaviour change, no CMake change, no code touched.** The
F5 item in `v0730_first_action_runbook.md` is a phantom work item. It is
closed and has been for a long time.

## What the runbook claimed

> **F5**: Bump `native-seventh-dispatch` boundary past v0727.
> `vf2_native_seventh_dispatch` does not yet exist in `CMakeLists.txt`
> (verified at v0730 - only `vf2_native_sixth_dispatch` is wired).

The "(verified at v0730)" is what makes it dangerous - it reads as a checked
fact.

## What is actually true

`vf2_native_seventh_dispatch` **did** exist. `seventh_dispatch_v0144.md`
records it:

> The seventh `fa_game_info` entry at `0x0001645c` / registry `0x00515200`
> matches the reference i960 state exactly. The sixth-to-seventh continuation
> adds 36 recovered blocks and 2,170 instructions, with exact CPU, condition
> state and mutable-memory equality at the target boundary.

It was then **deliberately retargeted**. `CHANGELOG.md` carries the entry:

> test: retarget `vf2_native_seventh_dispatch` to
> `vf2_native_eleventh_dispatch` (`native-nth-dispatch 11`); the strict
> sixth-dispatch base now ends at the tenth `fa_game_info` entry (`8`
> repeated scheduler entries, `870` blocks / `7,404,901` instructions), so
> target `7` failed closed with `Checkpoint represents dispatch 10` **while
> dispatches 7-10 are already covered per-block inside the sixth command** -
> the continuation now proves one further `37`-block / `2,166`-instruction
> cycle to dispatch 11 with exact CPU/memory/counter state.

So dispatches 7, 8, 9 and 10 are not missing. The sixth-dispatch test walks 8
repeated scheduler entries and compares **all 870 blocks** per-block against
the reference. The eleventh test then proves one further cycle beyond that.

## Reproduced at v0732e

```text
$ vf2i960 native-nth-dispatch roms/vf2 7
Checkpoint represents dispatch 10; target must be >= that value.   (exit 1)

$ vf2i960 native-nth-dispatch roms/vf2 11
Native continuation checkpoint: dispatch 10 at 0x0001645c
Native dispatch 11 validation: MATCH                             (exit 0)
```

The `7` failure is the **documented intended behaviour**, not a gap. The
arithmetic is in `command_native_continue_dispatch`
(`tools/vf2i960/main.c`):

```c
current_dispatch = (uint32_t)(runtime_state.scheduler_entries / 2u) + 6u;
```

With the sixth base's 8 entries that is `4 + 6 = 10`, so the continuation can
only target 11 and up.

## How the false premise happened

The v0730 author grepped `CMakeLists.txt` for the literal string `seventh`,
found nothing, and concluded the test had never been wired - without reading
the CHANGELOG entry that explains the rename. The claim then propagated into
two places: the runbook's F5 item and
`native_dispatch_boundary_v0730_measured.md` ("consistent with the
`native-sixth-dispatch` / `native-seventh-dispatch` ctest entries already on
master"). Both are corrected at v0732e.

This is the **third** v0730-handoff premise found false, after MANUAL
SETTING's location and the S1 reference. The pattern is worth naming: a
handoff that asserts a negative ("does not exist") needs the same evidence
standard as a positive one, and a literal-string grep is not evidence that a
test was never wired - a rename leaves no trace of the old name.

## What this means for the remaining queue

F1, F2, F3 (measured), F4 (measured) and **F5** are all accounted for. That
leaves:

- **F3 remainder** - the INDIVIDUAL value-row release, 4061/38, one sample,
  refused.
- **F4 remainder** - the INDIVIDUAL release render itself, whose digit cells
  and `runs[]` filter need a reference trace.
- **P1-P4** - the `fa_player` corridor.

`native_dispatch_endurance.md` already warns against the obvious next move:

> In the baseline observed scenario, simply running more frames has not
> exposed the next native boundary even after thousands of dispatches. Future
> frontier work should therefore prioritize controlled alternate inputs,
> fighter/task states, camera modes and other branch-inducing mutations
> rather than longer baseline endurance.

That is exactly the shape of the work that found the three v0732c corrections:
KICK instead of PUNCH, INDIVIDUAL instead of COMMON, non-default credit
vectors - controlled input and state mutations, not more frames.
