# Native differential lockstep

## Purpose

The native differential API is the reusable validation layer between recovered
C execution and the reference i960 executor. It removes the need for each
developer command to maintain its own list of recoverable instruction pointers
and its own block-by-block comparison loop.

The API is declared in `include/vf2/native_differential.h` and implemented in
`src/recovered/native_differential.c`.

## Contract

The caller supplies two independently allocated Model 2 machines and CPUs that
start from the same architectural and mutable-memory snapshot:

- the **reference** side executes original i960 instructions;
- the **native** side executes only `vf2_native_runtime_step` recoveries;
- native interpreter fallback is never permitted.

For every accepted native block, the runner:

1. records the `vf2_native_runtime_step_report`;
2. advances the reference i960 by exactly the recovered instruction count;
3. mirrors deterministic host-side frame-wait and vector-12 events when the
   native block represents a frame-wait phase;
4. verifies that reference and native frame-event states agree;
5. verifies that the reference instruction counter advanced by the reported
   recovered count;
6. captures complete snapshots of both CPUs and all mutable Model 2 memory;
7. compares architectural execution counters that are not serialized by the
   snapshot format; and
8. continues only when every compared state is identical.

A native unsupported path, reference execution failure, CPU or memory mismatch,
frame-event mismatch or block-budget exhaustion returns immediately with a
partial `vf2_native_differential_report`. The report preserves the last native
step, final addresses, cumulative instruction counts, minimum-block policy and
the first difference.

## Stop policies

`vf2_native_differential_run_until` retains the ordinary run-until contract. If
the CPU already points at the stop address, the call succeeds without executing
a block.

`vf2_native_differential_run_until_after` additionally accepts
`minimum_blocks`. The stop address is successful only after at least that many
native blocks have been executed and compared. `minimum_blocks` must not exceed
`max_blocks`.

This distinction is necessary for repeated scheduler cycles because the second
and third sweep boundaries are both `fa_game_info` at `0x0001645c`:

```c
vf2_native_differential_run_until_after(
    &reference_machine,
    &reference_cpu,
    &native_machine,
    &native_cpu,
    &native_state,
    UINT32_C(0x0001645c),
    1u,
    repeated_frame_block_budget,
    &report
);
```

The minimum of one block prevents an accidental zero-length success while
preserving the simpler API's useful zero-length behavior.

## `native-third-dispatch`

`vf2i960 native-third-dispatch <rom-directory>` first executes the already
validated startup, first sweep, post-frame bridge and second task-entry path.
At the synchronized second `fa_game_info` boundary, it creates a persistent
native runtime state and delegates the repeated-frame corridor to
`vf2_native_differential_run_until_after`.

The command reports the first unsupported native block or first exact CPU,
counter, frame-event or mutable-memory difference. The supported 36-file ROM
set has now validated the complete accepted corridor: it returned to the third
`fa_game_info` entry after 42 compared blocks and 55,239 instructions on both
sides. CPU state, local frames, execution counters, mirrored frame-event state
and every mutable Model 2 memory region matched at each checkpoint. The native
side used no interpreter fallback.

GitHub Actions cannot contain or execute the proprietary ROM set, so CI covers
the ROM-independent contracts while the ROM-backed acceptance is reproduced
with `vf2i960 native-third-dispatch <rom-directory>`.

## Tests

`tests/recovered/test_native_differential.c` covers:

- invalid arguments;
- a synchronized zero-length run;
- enforcement of a same-address minimum-block stop;
- rejection when `minimum_blocks > max_blocks`;
- initial mutable-memory divergence;
- initial architectural-counter divergence;
- initial instruction-pointer divergence;
- explicit block-budget exhaustion; and
- propagation of an unsupported native runtime address.

CMake registers `vf2_native_third_dispatch` only when the configured supported
ROM directory exists. The ROM-independent API and failure-contract tests run in
GCC, Clang, ASan, UBSan and LeakSanitizer CI.

<!-- temporary cleaned-master CI verification -->
