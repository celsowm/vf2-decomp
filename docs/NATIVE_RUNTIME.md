# Recovered native runtime

## Purpose

`vf2_native_runtime` is the reusable execution layer above the individual
recovered blocks. It is independent from the ROM-backed differential CLI and
never falls back to the Intel i960 interpreter.

The runtime currently routes six accepted execution classes:

- ordinary post-frame recovered bridge blocks;
- recovered task bodies;
- the two recovered frame-wait/interrupt phases;
- the recovered second scheduler entry;
- generic second-sweep scheduler transitions; and
- the recovered second-sweep scheduler epilogue.

Unknown instruction pointers and unobserved branches return
`VF2_ERROR_UNSUPPORTED` explicitly. A second hit of the main-loop scheduler
call site `0x0000a010` after the second sweep has been accounted dispatches
the same `vf2_hybrid_second_scheduler_enter` recovery: the recovered scheduler
scan is generic across sweeps, and `vf2i960 observe-third-sweep` confirmed the
architectural preconditions and task selection are identical for repeated
sweeps.

## API

Initialize persistent frame-event and accounting state:

```c
vf2_native_runtime_state runtime;
vf2_status status = vf2_native_runtime_initialize(&runtime, 4u);
```

Execute one accepted recovered block:

```c
vf2_native_runtime_step_report step;
status = vf2_native_runtime_step(&machine, &cpu, &runtime, &step);
```

Execute recovered blocks until a named boundary:

```c
vf2_native_runtime_run_report run;
status = vf2_native_runtime_run_until(
    &machine,
    &cpu,
    &runtime,
    stop_address,
    max_blocks,
    &run
);
```

`run_until` succeeds immediately when the CPU is already at the requested stop
address. Exhausting the block budget is reported as unsupported and leaves a
complete partial report.

## Accounting

Both per-step and cumulative reports retain:

- entry and exit addresses;
- recovered bridge or task kind;
- current and next scheduler task indices;
- current and next registry addresses;
- number of descriptors scanned;
- block, task, frame-wait, scheduler-entry, transition and finish counts;
- recovered instruction count; and
- recovered procedure calls and returns.

The cumulative state is modified only after a recovered block completes
successfully.

## Complete second scheduler sweep

The runtime now composes the entire observed second scheduler sweep:

1. execute `fa_game_info` at `0x0001645c`;
2. scan task descriptors 14 through 17 and enter recurring `fa_camera` at
   `0x0001d458`;
3. execute the recurring camera update and return to `0x00010dcc`;
4. enter and execute `fa_user`;
5. skip six inactive descriptors and enter recurring `fa_sound` at
   `0x00043abc`;
6. execute its deterministic buffer transfer and return;
7. enter recurring `fa_kill_osage`;
8. enter and execute `fa_osage0` and `fa_osage1`; and
9. account the last active task, scan the final inactive descriptor and return
   from the scheduler to the main loop at `0x0000a014`.

The transition executor reads each registry stride from offset `+0x08`. It does
not assume a fixed registry size and skips inactive descriptors until it reaches
the next active task.

The extension from the second `fa_game_info` entry to `0x0000a014` contains:

- **14** recovered runtime blocks;
- **566** original i960 instructions reproduced by C;
- **12** recovered procedure calls;
- **14** recovered procedure returns; and
- zero native-side interpreter fallbacks.

## Task variants

The recurring camera path composes the existing recovered update and post-update
blocks while preserving the live fighter cursor that differs from the first
camera invocation.

The recurring sound path copies the observed queued word into the global sound
state, clears the source and control fields, and returns in 20 instructions.

The recurring `fa_kill_osage` path shares the already recovered memory and
register semantics but records the observed 33-instruction recurring profile,
rather than the 36-instruction first-dispatch profile.

## Validation

The ROM-backed differential run compared complete CPU state, architectural
local-register frames and all mutable Model 2 memory after each of the 14 blocks.
Every phase reached `MATCH`.

ROM-independent tests cover:

- initialization and zero-length execution;
- a complete recovered bridge procedure;
- second `fa_game_info` execution;
- dynamic registry-stride scanning across inactive descriptors;
- the second-sweep scheduler epilogue;
- unsupported routing; and
- block-budget exhaustion.

The warning-as-error build and the six ROM-independent test targets used during
this increment passed. The native runtime and scheduler tests were also run
under AddressSanitizer and UndefinedBehaviorSanitizer.

## Next integration

The current continuous native boundary is the main-loop continuation at
`0x0000a014`. The next target is to route the already recovered geometry,
texture, frame timer and interrupt blocks through `vf2_native_runtime`, execute a
second complete frame boundary and reach the third scheduler sweep without
native-side interpretation.

<!-- temporary clean-master validation marker for v0.2 late-sweep scheduling -->
