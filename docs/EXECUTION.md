# Deterministic i960 execution

The executor validates recovered C and discovers the next evidence boundary. It
is not intended to become the native game runtime.

The same rule applies to the public game facade: `vf2_game_run_native_frame`
is a validation/oracle entry point, while `vf2_game_update` must not execute
i960 instructions. Until the portable gameplay pipeline is complete, an
attached oracle runtime makes `vf2_game_update` fail closed with
`VF2_ERROR_UNSUPPORTED`.

## Accepted differential checkpoints

| Original path/function | Accepted C | Result |
|---|---|---|
| `0x000000b0` → `0x000001b0` | `vf2_recovered_boot_stage1_execute` | MATCH |
| `0x000001b0` → `0x0000052c` | `vf2_recovered_boot_stage2_execute` | MATCH |
| function `0x00010cbc` | `vf2_recovered_task_registry_initialize` | MATCH |
| interrupt handler `0x00000d50` | `vf2_recovered_timer_irq_dispatch` | MATCH |
| interrupt handler `0x00000d30` → `0x00000040` | `vf2_recovered_interrupt_ack_dispatch` | UNIT MATCH |

The timer comparison enters the same architectural interrupt frame on both
machines, interprets the original handler on one side and executes accepted C
on the other, then compares every modeled mutable region.

The `0x00000d30` row is an isolated four-instruction differential fixture. It
is accepted only for an already-entered procedure frame and is not yet claimed
as a complete interrupt-dispatch corridor; the neighboring `0x00000e10` and
`0x00000e30` handlers remain unsupported.

## External interrupts

`vf2_i960_cpu_enter_interrupt` follows the PRCB state established by VF2:

1. read the interrupt table from `PRCB + 20`;
2. read the interrupt stack from `PRCB + 24`;
3. resolve the vector handler at `table + 36 + (vector - 8) * 4`;
4. create a type-7 local-register frame;
5. save process control, arithmetic control and vector below the new FP;
6. enter supervisor/interrupt state;
7. restore CPU state when the handler executes `ret`.

The Model 2 interrupt controller implements request, enable and acknowledge
semantics. A write to the request register preserves only requested bits present
in the written mask, matching the board driver's acknowledgement behavior.

## Runtime synchronization pass

```sh
build/vf2i960 scheduler-pass roms/vf2
```

The command performs these evidence-backed events:

1. execute to wait loop `0x0004aff8`;
2. raise timer request bit 5;
3. enter vector 14 at `0x00000d50`;
4. reload timer 3 and set the work-RAM wait flag;
5. leave the wait routine and return to `0x0004b07c`;
6. raise frame request bit 0;
7. enter vector 12 at `0x00000bc0`;
8. execute its idle path and return.

Expected measurements for the supported set:

```text
Checkpoint instructions:       2985244
Interrupt handler instructions:33
Wait release instructions:     8
Frame interrupt instructions:  28
Interrupt entries/returns:     2/2
```

This is not yet a complete task-dispatch pass. The frame handler's runtime-ready
condition is false, so the non-idle calls and scheduler consumer are skipped.

## Commands

```sh
build/vf2i960 execute roms/vf2
build/vf2i960 runtime-checkpoint roms/vf2
build/vf2i960 scheduler-pass roms/vf2
build/vf2i960 compare-timer-irq roms/vf2
build/vf2i960 compare-boot roms/vf2
build/vf2i960 compare-init roms/vf2
build/vf2i960 compare-task-registry roms/vf2
```

## Failure evidence

On unsupported behavior, execution reports the halt instruction, the previous
32 instructions, all integer registers, frame depth and region checksums.

## Extension rule

1. preserve the exact trace and state;
2. verify semantics using primary i960 documentation and a reference implementation;
3. add a focused ROM-independent test;
4. implement only proven behavior;
5. rerun all differential and runtime checkpoints.

## v0.0.8 scheduler-dispatch boundary

`scheduler-dispatch` reaches the runtime checkpoint, injects evidence-backed
timer/frame events, observes the natural set of `0x00500068[31]`, builds the
recovered scheduler plan and confirms each initially runnable descriptor as the
original dispatcher enters it. A dispatch is identified by both its entry point
and the current registry pointer in `g13`, which distinguishes tasks sharing
code. The command stops after all seven initial entries are observed.


## v0.0.9 entry-to-return profiling

`task-profile` follows the same natural runtime transition as
`scheduler-dispatch`, but it does not stop at each entry. It records the task
frame depth and runs until that frame returns to the scheduler. Four mutable
regions are diffed at each boundary: work RAM, buffer RAM, geometry RAM and
the coprocessor function port.

For accepted C paths, the command captures the live entry snapshot, restores
it into a second machine, executes the recovered C function there and compares
mutable memory after the original task returns. Five observed entries match.
See `FIRST_DISPATCH_TASKS.md`.

## v0.0.10 camera-prefix and osage-cleanup validation

`compare-first-dispatch` extends the live-entry snapshot comparison to all seven
initial scheduler entries. Complete or bounded task paths are compared at
procedure return. `fa_camera` is compared earlier, when the original reaches
continuation `0x0001d458`, because only its initialization prefix is accepted C.

The profiler also records call sites and resolved dynamic targets. This exposes
all six first-dispatch camera calls, including the indirect mode-table call at
`0x0001d4d4`, without claiming the later camera update logic as recovered.

## v0.0.11 recurring camera-prefix validation

The camera clone is now checked twice. The first comparison remains at
`0x0001d458`. After that match, the recovered machine executes
`vf2_recovered_task_camera_first_update`; the original continues until
`0x0001d660`, where modeled memory is compared again.

The second interval covers complete helper `0x000214dc`, the observed early
exit of `0x00020558`, fighter-profile global selection and the observed early
exit of `0x0001fc00`. The comparison is memory-only: hybrid replacement of the
original instructions is deferred until register-state equivalence is also
proven. `compare-camera-classifier` separately invokes helper `0x000214dc`
with nine input combinations and compares the returned `g0` value against the
portable C implementation.


## v0.0.12 camera post-update gate validation

After the recurring-prefix match at `0x0001d660`, the recovered clone executes
`vf2_recovered_task_camera_post_update_gate`. The original path reads input
flags `0x0006`, skips the input-bit-3 viewport block and branches through
control byte `0x0050009c = 1` to the return at `0x0001e524`. Modeled memory is
compared for a third camera boundary.

The C helper also implements the complete non-viewport task-flag path through
`0x0001d984`. Unit tests cover the fast exit, normal flag derivation, mode/phase
overrides and explicit rejection of the unrecovered viewport branch. After the
seventh task returns, the profiler records stable scheduler checkpoint
`0x00010dcc`.

## v0.0.13 viewport differential boundary

`compare-camera-viewport` forces input bit 3 in a private copy of the program
ROM, creates two deterministic task-entry states and executes the original block
from `0x0001d678` to `0x0001d8e8`. The same entry snapshot is restored into a
second machine and evaluated by recovered C.

The fixed state selects the literal 8-entry and 10-entry tables. The calculated
state exercises all three recovered helpers, fighter-profile/weight updates,
normalization and ROM-backed lookup-table interpolation. Every modeled mutable
region is compared, including the coprocessor scratch port.

This test does not claim that the real first dispatch enters the viewport path;
its observed flags remain `0x0006`, with bit 3 clear.

## v0.0.14 hybrid first-dispatch validation

v0.0.14 first composed the three accepted camera memory blocks while using an
independent original execution to supply register postconditions. That bridge
proved the memory intervals and the final scheduler checkpoint.

## v0.0.15 stateful hybrid first-dispatch validation

`hybrid-first-dispatch` still runs an original reference machine and a recovered
machine independently from the live `fa_camera` entry. The difference is that
`vf2_hybrid_camera_execute` now produces the recovered machine's complete
architectural post-state itself. No CPU field is copied from the reference.

The three replacements are:

- `0x0001d320 -> 0x0001d458`, 2,586 original instructions;
- `0x0001d458 -> 0x0001d660`, 107 original instructions;
- `0x0001d660 -> 0x0001e524`, 6 original instructions.

After every block, complete snapshots are compared: CPU controls, integer
registers, active local frames and all modeled mutable regions. Instruction,
call and return counters are separately required to match. One original camera
`ret` remains interpreted before both executions continue to scheduler
checkpoint `0x00010dcc`.

The original execution is now only a differential oracle. It is never used to
synchronize recovered state. See `docs/HYBRID_EXECUTION.md`.

## v0.0.16 native first-dispatch validation

`native-first-dispatch` executes all seven initial task bodies and six
scheduler transitions in recovered C, validating 4,342 instructions through
checkpoint `0x00010dcc`.

```bash
build/vf2i960 native-first-dispatch /path/to/vf2
```

## v0.0.17 persistent second-dispatch validation

`native-second-dispatch` additionally invokes
`vf2_hybrid_first_dispatch_scheduler_finish`, which accounts the final runnable
record, scans descriptor 28, reproduces diagnostic state and returns to
`0x0000a014`. Reference and native machines then execute the same post-frame
bridge without any snapshot restore. A frame IRQ releases the wait at
`0x00010f98`, and both machines reach the next scheduler traversal and the
second `fa_game_info` entry with complete state equality.

```bash
build/vf2i960 native-second-dispatch /path/to/vf2
```

The first sweep contributes 4,623 recovered instructions. In v0.0.17 the
bridge contributed 1,270,822 interpreted instructions.

## v0.0.18 post-frame texture bridge validation

v0.0.18 first replaced the repeated byte/word inner runs, recursive tree
expansion and color conversion, recovering 712,821 of 1,270,822 bridge
instructions.

## v0.0.19 near-native post-frame bridge validation

v0.0.19 raises the replacement boundary from inner runs to complete decoder and
table-building procedures. `native-second-dispatch`,
`compare-texture-bridge` and `compare-post-frame-bridge` now prove:

```text
Post-scheduler bridge instructions: 1270822 total
Recovered bridge instructions:      1262476
Interpreted bridge instructions:    8346
Recovered bridge blocks:            48
Differential memory checkpoints:    48
  texture byte decoders:            4
  texture word decoders:            4
  symbol-table builders:            4
  pair-table builders:              4
  texture tree expansions:          4
  texture color conversions:        28
Frame-wait threshold:               4 visits
Frame interrupts injected:          1 (vector 12)
First geometry instruction:         0x00002eec
First geometry write target:        0x00803008
First changed geometry byte:        0x00803009
Final CPU and memory state:         MATCH
```

The byte decoder at `0x0004c6e0` reproduces row handling, direct symbols,
palette deltas, RLE, recursive handler dispatch and bitstream refill. The word
decoder at `0x0004cc28` reproduces complete literal/RLE rows. The preceding
builders at `0x0004c3f0` and `0x0004c4d4` reconstruct their symbol and pair
tables from the compressed stream.

Frame synchronization is controlled by `vf2_hybrid_frame_wait_observe` rather
than ad-hoc validation-loop logic. It injects vector 12 after the fourth visit
to the accepted wait addresses.

The instruction at `0x00002eec` is the first proved geometry-facing store. It
targets `0x00803008`, establishing the next recovery boundary. The remaining
8,346 instructions are still interpreted and must not be described as a fully
native bridge.

## v0.0.20 geometry-boundary validation

v0.0.20 expands the accepted bridge from 48 to 135 procedure-level blocks:

```text
Post-scheduler bridge instructions: 1270822 total
Recovered bridge instructions:      1268004
Interpreted bridge instructions:    2818
Recovered bridge blocks:            135
Differential memory checkpoints:    135
Recovered bridge calls/returns:     229/272
First geometry instruction:         0x00002eec
First geometry write target:        0x00803008
Final CPU and memory state:         MATCH
```

New recovered procedures include address-table construction, diagnostic text
copy, tile-glyph expansion, palette-page upload, texture-conversion loop
control, timer/wait update, video-status latch, frame scratch clear and the
first geometry frame commit/setup sequence.

The commit procedure writes the previous ring pointer to `0x00803008`, reads
board progress from `0x00802008`, advances a four-entry command ring and writes
the next pointer to `0x00801008`. This is a recovered register protocol for the
observed path, not yet a polygon or TGP command decoder.

```bash
build/vf2i960 compare-geometry-boundary /path/to/vf2
```


## v0.0.21 second scheduler-entry validation

The bridge validator now substitutes the complete interval from `0x0000a010`
through the second `fa_game_info` entry. The recovered block scans fourteen
descriptors, rejects unsupported alternate ready states and reconstructs the
scheduler frame before the final `callx`.

```text
Second scheduler entry validation: MATCH
Recovered instructions:            235
Descriptors scanned:                14
Inactive descriptors:               13
Selected task index:                13
Selected registry:                  0x00515200
Selected entry:                     0x0001645c
Final CPU and memory state:          MATCH
```

```bash
build/vf2i960 compare-second-scheduler-entry /path/to/vf2
```


## v0.0.22 gameplay/geometry-helper validation

The bridge dispatcher now substitutes four helper families around the first
geometry boundary. The accepted execution contains one inline diagnostic
thunk, four texture-status lines, three direct game-state classifications and
eight color/control lookups. All are compared with complete CPU and memory
checkpoints.

```text
Recovered bridge instructions:      1268752
Interpreted bridge instructions:        2070
Recovered bridge blocks:                 143
Recovered bridge calls/returns:       250/297
Final CPU and memory state:             MATCH
```

```bash
build/vf2i960 compare-game-geometry-helpers /path/to/vf2
```


## v0.0.23 texture orchestrator profile

This release is pure evidence. No new recovered block was added. The four
v0.0.22 helper families still drive the same per-kind counts (1/4/3/8). The
headline totals are reproduced unchanged and the strict-equality assertions
in `tools/vf2i960/main.c:4305-4322` enforce that the native second-dispatch
did not regress.

```text
Recovered bridge instructions:      1268752
Interpreted bridge instructions:        2070
Recovered bridge blocks:                 143
Recovered bridge calls/returns:       250/297
Final CPU and memory state:             MATCH
```

A read-only developer instrument was added for observing the texture
orchestrator cluster (`[0x0004bb18, 0x0004c180]`):

```bash
build/vf2i960 trace-orchestrator /path/to/vf2
build/vf2i960 trace-orchestrator /path/to/vf2 path/to/orchestrator_trace.csv
```

The command reuses `command_native_dispatch` and writes one CSV row per
interpreted native step whose `ip_before` lies in the orchestrator cluster.
It refuses to emit usable evidence if the strict total assertions do not all
hold, which keeps it provably non-behavior-changing relative to v0.0.22.
The default CSV path is
`decomp/i960/notes/texture_orchestrator_v0023.csv`. Observations are recorded
as evidence only -- no source file reads the CSV.


## v0.0.24 zero-interpreted second dispatch

This release achieves 0 interpreted instructions in the post-frame bridge! The accepted startup path from the end of the first scheduler sweep through the second entry into `fa_game_info` now executes entirely as recovered C.

```text
Recovered bridge instructions:      1270824
Interpreted bridge instructions:           0
Recovered bridge blocks:                 192
Recovered bridge calls/returns:       342/340
Final CPU and memory state:             MATCH
```

```bash
build/vf2i960 native-second-dispatch /path/to/vf2
```

The v0168 audit closed two architectural `ret` handoffs that were still being
stepped by the reference executor at `0x0004bab4` and `0x000020ec`. They are
now explicit one-instruction recovered `return-stub` bridges, preserving the
original block boundaries while the strict second-dispatch path remains fully
recovered. The repeated game-state path also models the measured selector-
dependent condition state at the `0x000020ec` boundary.


## v0.0.25 consolidation phase

The consolidation phase focuses on:
- Updating all project-wide version, status, and roadmap documents to be fully consistent and accurate;
- Reorganizing and modularizing the single-file post-frame bridge (`src/recovered/texture_bridge.c`) into named subsystems: `texture`, `video`, `geometry`, `input`, `match`;
- Mapping unobserved/uncovered branches to `docs/UNCOVERED_BRANCHES.md`;
- Adding multi-frame unit tests validating repeat frame execution and interrupt handling.
