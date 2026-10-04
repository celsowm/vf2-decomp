# Frame-advance workflow for the reference executor (v0731)

This note establishes the **frame-advance workflow** that
`decomp/i960/notes/fa_player_19ef8_live_v0309.md:44` and
`decomp/i960/notes/fighter_candidate_dual_base_v0702.md:36` both recorded
as unestablished:

> "Reference-only `vf2probe` from the same park still spins in the
> `0x10fa0` wait loop (no IRQ injection). `--raise-irq` does not break that
> loop from this park."
>
> "F1's deep pass needs a frame advance, for which no probe workflow is
> established (`--raise-irq`/`--enter-interrupt` vectors unknown). That
> workflow is the prerequisite, not more same-park traces."

It is now established. The blocker was not the tooling — it was not knowing
which mask/vector to inject and which addresses recur.

## 1. The wait loop

`vf2i960 disasm roms/vf2 0x00010f08 34` decodes the scheduler's frame wait.
The spinning pair is:

```text
00010f74  ldob 0x0050006d, g0        ; secondary wait flag
00010f7c  cmpobe 1, g0, 0x00010fa4   ; exit if == 1
00010f80  cmpobe 2, g0, 0x00010f90   ; alternate entry if == 2
00010f84  ldob 0x00500000, g0        ; sample the frame byte
00010f8c  cmpobe 1, g0, 0x00010fc4   ; exit if == 1
00010f90  ldob 0x00500000, g0
00010f98  ldob 0x00500000, r3        ; <- spin
00010fa0  cmpibe r3, g0, 0x00010f98  ; loop while unchanged
```

The loop re-reads `0x00500000` every iteration and branches back while the
value is unchanged. Nothing in the reference executor writes that byte, so
an unassisted run spins indefinitely. Measured: from the parked state,
`--max-steps 200000` and `--max-steps 5000000` both terminated with
`halt_reason: "maximum steps"`, `ip = 0x00010f98`, and **zero** additional
procedure calls (11558 -> 11558) over 4.8M instructions. That is a pure
spin, confirming the notes.

The release path is the frame IRQ. `src/recovered/frame_wait.c` already
recovers exactly this loop and documents the injection:

```c
#define VF2_FRAME_WAIT_EARLY   0x00000f7c
#define VF2_FRAME_WAIT_MAIN    0x00010f98
#define VF2_TEXTURE_INIT_WAIT_POLL 0x0004afe4
#define VF2_FRAME_INTERRUPT_MASK   1
#define VF2_FRAME_INTERRUPT_VECTOR 12
#define VF2_FRAME_INTERRUPT_LEVEL  1
```

`vf2_hybrid_frame_wait_execute` polls `0x00500000`, and once
`state->visits >= state->visits_before_interrupt` it calls
`vf2_model2a_raise_interrupt(machine, 1)` and
`vf2_i960_cpu_enter_interrupt(cpu, machine, 12, 1)`.

## 2. The probe equivalent

`vf2probe` exposes the same two operations, so the reference executor can
reproduce the hardware effect directly:

```sh
build/Debug/vf2probe.exe --rom-dir roms/vf2 --snapshot <park.vf2snap> \
  --raise-irq 0x1 --enter-interrupt 12=1 --until <address> \
  --output-snapshot <next.vf2snap>
```

Measured on the parked COIN ASSIGNMENT state `out/f1-ring-1-tap.vf2snap`
(`ip = 0x10d54`, `0x500000 = 0`, `0x50006d = 0`):

| leg | stop | result |
| --- | --- | --- |
| no injection | `0x00010f98` | 200k / 5M steps, still spinning, `0x500000` stays 0 |
| inject, wait for next wait | `0x00010f98` | **1697 instructions**, `ip = 0x10f98` |
| inject, wait for frame entry | `0x00000530` | **980 instructions**, `ip = 1328 (0x530)` |

The injection is provably effective: `0x00500000` goes `0 -> 1` and the
procedure counters advance by 15 calls / 15 returns as the interrupt
handler runs and returns.

## 3. The chaining rule

Two addresses recur once per frame, and only these two are usable chain
points:

- `0x00010f98` — the frame wait.
- `0x00000530` — the bridge frame entry (the step after the `0x00009ff8`
  diagnostic call).

Anything else is a dead end:

- **`--until 0x0000a010` does not work for chaining.** `0xa010` is a return
  address inside the frame, not a per-frame boundary. A snapshot captured
  with `--until 0x0000a010` resumes at a scheduler address
  (`ip = 0x10d54`), and a subsequent leg re-stopping at `0xa010` can halt
  with `run_instructions: 0`. The v0727 `ca-*` captures are at `ip = 0x530`
  for exactly this reason.
- **`--until 0x00009ff8` never fires.** From a post-frame park it ran the
  full 200,000-step limit and ended at `ip = 0x10fe8` without reaching it.
- **Patching `0x0050006d` is not a substitute.** Setting it to 1 releases
  one pass, but the runtime resets the flag and re-enters the wait, so the
  spin resumes. Only the IRQ release is durable.

So the reliable leg shape is: **frame entry -> patch latches -> `--until
0x00010f98` -> inject -> `--until 0x00000530`**, which advances exactly one
game frame per pair and always lands on a recurring address.

## 4. Why this matters

This removes the prerequisite that three prior notes
(`coli_recurring_hunt_v0269`, `fa_player_19ef8_live_v0309`,
`fighter_candidate_dual_base_v0702`) flagged as blocking further recovery
depth. Any F-slice or P-slice that needs a frame advance past a park can
now take frame steps instead of giving up.

The established bodies still hold, so the workflow composes with the
existing v0727 evidence: from a frame-entry snapshot with the settled idle
latch, one bridge frame is 4420-4421 instructions (232 prefix + 4188/35
body), the SERVICE/DOWN tap is 4426 (232 + 4194/34) and the TEST edge is
4637 (232 + 4405/38).

## 5. Scope and honesty notes

- This note establishes the *workflow*, not a differential proof. The
  interrupt injection models a hardware edge; it is not game semantics, and
  the native runtime performs the same injection from its own recovered
  `frame_wait.c`. A chained strict differential must therefore compare the
  native runtime against a reference run driven with the **same** single
  injection per wait, or the comparison is invalid by construction.
- `--raise-irq` is documented in `coli_recurring_hunt_v0269.md` as
  "raise-then-enter after restore/mutations, mirroring `commands.c` order".
  The mask/vector pair `1 / 12 / level 1` was simply not recorded. It is
  `VF2_FRAME_INTERRUPT_MASK` / `VECTOR` / `LEVEL` in `frame_wait.c`.
- `visits_before_interrupt` (the VBlank visit phase) is **not** reproduced
  by a single injection at restore: the native runtime lets the loop poll
  N times before injecting, while the probe injects immediately. Any slice
  whose correctness depends on the exact phase (the `phase_seed`
  reconstruction at `frame_wait.c:188` reads it) must reconstruct or
  measure the phase rather than assume one injection equals the native
  path. The COIN ASSIGNMENT menu frames measured here are inside a single
  bridge frame and do not cross the wait, so they are unaffected.

## 6. Anti-traps

- Do not report a frame advance from a run that reached the step limit at
  `0x10f98` with unchanged procedure-call counters; that is the spin, not
  progress. Check `procedure_calls` actually moved.
- Do not use `0xa010` or `0x9ff8` as chain stops. Use `0x10f98` and
  `0x530`.
- Do not use `--trace` output to find recurring addresses without noticing
  `ip_before` is decimal, not hex.
- Do not treat a one-shot injection as equivalent to the native
  `visits_before_interrupt` phase; measure the phase when it matters.
