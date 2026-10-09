# v0763: per-step loop UNSUPPORTED path hardened (frame pop)

The per-step loop in `hybrid_execute_interpreted_until` (hybrid.c:571-625)
now pops the i960-call frame when a registered hook refuses, instead of
trusting the recovery's `cpu->ip` to be at the call's entry IP. This is
the foundational fix that makes the v0761-style "refused hook leaves
cpu->ip at an internal ret slot" scenario safe.

## What changed

In `src/recovered/hybrid.c`, the per-step loop's UNSUPPORTED branch now
does:

```c
} else if (status == VF2_ERROR_UNSUPPORTED) {
    /* Undo the i960 `call` the per-step loop already stepped:
     * pop its frame and continue from the post-call IP. */
    vf2_status return_status =
        vf2_i960_cpu_return_procedure(cpu, machine);
    if (return_status != VF2_OK) {
        return return_status;
    }
    status = VF2_OK;
    continue;
}
```

Previously the UNSUPPORTED branch was a bare `continue;` with a comment
claiming "cpu->ip is at the entry IP; the next iteration will not
re-fire." That comment was wrong: the recovery can set `cpu->ip` to any
post-instruction IP (e.g. 0x20050 for 0x1fcc0's inlined 0x1fffc refusal
of 0x2c38), and the next iteration's `vf2_i960_step` from that IP
executes a `ret` that pops the wrong frame, after which the per-step
loop's stepping never reaches the configured `stop_address` and the
CTest for the live leg times out at 600 s (see v0761 retraction note).

The new branch:

1. Calls `vf2_i960_cpu_return_procedure(cpu, machine)`, which:
   - Decrements `local_frame_depth`.
   - Restores the caller's local-frame registers (including `r2`).
   - Sets `cpu->ip = cpu->registers[2]` — the post-call IP
     (call-site + 4, exactly what the i960 `call` instruction
     stored in `r2` as the return address).
2. Sets `status = VF2_OK` so the per-step loop's `while` condition
   stays true and the next iteration can step from the post-call IP.
3. Continues the loop.

The fix is generic: every existing refused hook (0xcf04) and every
future refused hook is now safe.

## Why status = VF2_OK

Without the explicit `status = VF2_OK`, the per-step loop's `while`
condition `status == VF2_OK && cpu->ip != stop_address && ...` is false
on the next iteration (status is still `VF2_ERROR_UNSUPPORTED` from
the recovery), and the loop exits via the trailing
`return cpu->ip == stop_address ? VF2_OK : VF2_ERROR_UNSUPPORTED;`
with cpu->ip = the post-call IP and stop = whatever, returning
UNSUPPORTED. That is exactly the bug the v0763 unit test caught on
its first iteration: the post-call IP != stop, the trailing return
returns UNSUPPORTED, the test fails. The v0763 commit message and
the on-disk `src/recovered/hybrid.c` both set `status = VF2_OK` after
the pop.

## Test

`tests/recovered/test_per_step_loop_unsupported_pop.c` (ctest #58, 0.04 s)
drives the per-step loop directly with a hand-constructed state:

1. A fake main_rom (0x80000 bytes) with a 12-byte program at 0x1000:
   ```
   0x1000: b 0x1008            (skip past to the call)
   0x1008: call 0x0000cf04     (call the registered 0xcf04 hook)
   0x100c: b 0x00010000        (unconditional branch to the stop)
   ```
2. A CPU with `cpu->ip = 0x1000`, frame pushed (return-address
   `0x1014` for the test wrapper frame), `procedure_calls = 0`,
   `procedure_returns = 0`.
3. Per-hook counters reset.
4. Call `vf2_hybrid_run_interpreted_until(0x1000, 0x10000)`.

The test asserts:

- `status == VF2_OK` (per-step loop terminates successfully).
- `cpu->ip == 0x10000` (the unconditional branch's target, equal
  to the configured stop).
- `hook_counts[0xcf04] == 1` (the registered hook fired exactly
  once).
- `cpu->procedure_calls - start_calls == 1` (the i960 `call 0xcf04`
  pushed exactly one frame).
- `cpu->procedure_returns - start_returns == 1` (the per-step
  loop's `return_procedure` popped exactly one frame, balancing
  the call).

The per-step loop's `hybrid_range_has_hooks(0x1000, 0x10000)` is
true (0xcf04 is in `[0x1000, 0x10000)`), so the per-step variant
is used. The hook for 0xcf04 fires at cpu->ip = 0xcf04, the
v0747 recovery refuses at cpu->ip = 0xcfb8, the v0763 fix pops
the i960-call frame, sets cpu->ip = 0x100c, sets status = OK, and
continues. The next iteration's `vf2_i960_step` at 0x100c decodes
the `b 0x10000`, jumps to 0x10000, and the per-step loop exits
with status = VF2_OK and cpu->ip = 0x10000.

## Why the cf04 path worked without this fix

The 0xcf04 recovery refuses at `cpu->ip = 0xcfb8` — the `ret` slot
of 0xcf04. Before this fix, the per-step loop's next iteration's
`vf2_i960_step` executed that `ret`, which correctly popped the
i960-call's frame for 0xcf04 (because the i960 `call 0xcf04`
instruction had pushed it). Net effect: `cpu->ip = call-site + 4`,
`procedure_returns` incremented, identical to the post-fix path.
The F4 and fifth/sixth-dispatch tests exercise the 0xcf04 hook in
real scenarios and stayed green before this fix.

The 0x1fcc0 path is different. The 0x1fcc0 recovery (v0755g)
inlines all 6 sub-callees and refuses at `cpu->ip = 0x20050` — the
`ret` slot of the inlined 0x1fffc, not 0x1fcc0 itself. Before this
fix, the per-step loop's next iteration's `vf2_i960_step` from
0x20050 executed the `ret`, which popped the i960-call's frame
for 0x1fcc0 (NOT for 0x1fffc, which the inlined 0x1fffc recovery
never actually called). The popped frame was the i960 `call 0x1fcc0`
frame, not the inlined 0x1fffc's frame. The CPU state became
inconsistent; the per-step loop's stepping either matched another
hook or never reached the configured stop; ctest timed out at 600 s.

The v0763 fix makes both paths safe: `return_procedure` always
restores the caller's saved IP (g14 / r2 = call-site + 4), so the
post-call IP is exactly the IP the original i960 would have
returned to.

## Validated

- New ctest entry `vf2_per_step_loop_unsupported_pop` (ctest #58, 0.04 s)
  PASSES.
- All 145 existing tests pass at v0760 + retraction + v0763 state:
  - `vf2_f4_individual_release` (ctest #145, 18.08 s) — exercises
    0xcf04 in the per-step loop on a real F4 snapshot.
  - `vf2_native_fifth_dispatch` (ctest #121, 15.14 s).
  - `vf2_native_sixth_dispatch` (ctest #122, 14.52 s).
  - All 14 player function unit tests (ctest #38-#49, #52-#55) PASS.
  - `vf2_callee_hook_fires_native` (ctest #59, 0.03 s) PASS.
  - `vf2_callee_hook_counters` (ctest #57, 0.03 s) PASS.
- `git diff --stat` shows the targeted edit:
  - `src/recovered/hybrid.c`: 1 block, ~25 lines changed
    (UNSUPPORTED branch).
  - `tests/recovered/test_per_step_loop_unsupported_pop.c`:
    new file, ~270 lines.
  - `CMakeLists.txt`: 5 lines (add_executable + add_test).

## Files

- `src/recovered/hybrid.c` — UNSUPPORTED branch in
  `hybrid_execute_interpreted_until`.
- `tests/recovered/test_per_step_loop_unsupported_pop.c` — ctest
  unit test.
- `CMakeLists.txt` — wire the test.
- `decomp/i960/notes/per_step_loop_unsupported_hardened_v0763.md`
  (this file).

## What v0763 enables (future)

With this fix in place, the v0761 follow-up becomes possible: wire
0x1fcc0 into `g_callee_hooks[]` again. The per-step loop's UNSUPPORTED
path now correctly pops the i960-call frame and continues from
g14 = call-site + 4, so a recovery that refuses at any internal
`ret` slot (e.g. 0x20050) is safe.
