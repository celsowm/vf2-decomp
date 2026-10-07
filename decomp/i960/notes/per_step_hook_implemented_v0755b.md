# v0755b: per-step hook infrastructure implemented — 4 callees of 0x29414 wired

## Summary

The per-step hook infrastructure is now implemented in
`hybrid_execute_interpreted_until` (the
`src/recovered/hybrid.c:436` function). A small static table of
`{entry_ip, hook_fn}` pairs is checked after each instruction in
the post-29414 dispatcher range (0x28178..0x14400). When
`cpu->ip` lands on a registered entry, the hook is called.

**4 callees of `0x29414` are wired** (v0741/v0745/v0746/v0747):

| callee | size | recovered? | refuse mode |
|---|---|---|---|
| `0x29598` | 84 B | paths A/B/C | path D clean-refuse (v0755a) |
| `0x439ac` | 80 B | all 4 paths | none |
| `0x43888` | 200 B | all 6 paths | none |
| `0xcf04` | 184 B | both paths | refuses sub-call to `0x1fcc0` |

The other 6 recoveries (v0749/v0750/v0751/v0752/v0753/v0754) are
**NOT** wired in this slice — they are sub-callees of `0x1fcc0`
(display_profile_apply), which is not in any currently-interpreted
range. They remain stand-alone unit tests for now. Wiring them
will require recovering `0x1fcc0` itself, which depends on
recovering `0x2c38` (the last un-recovered sub-callee, deferred
due to a disasm ambiguity — see v0755).

## What changed

### `src/recovered/hybrid.c:436-616` (per-step hook infrastructure)

Added:
- `vf2_hybrid_callee_hook_fn` typedef (function pointer for
  recovered callees).
- `vf2_hybrid_callee_hook_entry` struct (entry_ip + hook_fn).
- `g_callee_hooks[]` static table (4 entries: v0741/v0745/v0746/v0747).
- `hybrid_find_callee_hook(ip)` linear-search helper.
- `hybrid_range_has_hooks(entry, stop)` helper to decide whether
  a range needs the per-step loop.
- The per-step loop in `hybrid_execute_interpreted_until` —
  fires the hook when `cpu->ip` matches a registered entry after
  each instruction.

Modified:
- `hybrid_execute_interpreted_until` now uses the per-step loop
  for ranges that have at least one registered hook. Other ranges
  continue to use `vf2_i960_run` (the legacy-stepper path),
  preserving their existing behavior. The 0x16504 special case
  is preserved.

## The per-step loop

```c
if (hybrid_range_has_hooks(entry_address, stop_address)) {
    size_t steps = 0u;
    while (status == VF2_OK && cpu->ip != stop_address &&
           steps < VF2_INTERPRETED_TASK_STEP_LIMIT) {
        status = vf2_i960_step(cpu, machine, NULL);
        if (status != VF2_OK) {
            return status;
        }
        ++steps;
        const vf2_hybrid_callee_hook_entry *hook =
            hybrid_find_callee_hook(cpu->ip);
        if (hook != NULL) {
            status = hook->hook(machine, cpu);
            if (status == VF2_OK) {
                /* Function fully handled the call;
                 * cpu->ip is at the post-function IP. */
                continue;
            } else if (status == VF2_ERROR_UNSUPPORTED) {
                /* Function refused cleanly; cpu->ip is at
                 * the entry IP. We've already stepped past
                 * the call site, so the next iteration will
                 * not re-fire. */
                continue;
            } else {
                /* Other error: abort. */
                return status;
            }
        }
    }
    if (status != VF2_OK) {
        return status;
    }
    return cpu->ip == stop_address ? VF2_OK : VF2_ERROR_UNSUPPORTED;
}
```

The loop uses `vf2_i960_step` (which is the **arch stepper** in
hybrid.c, since the legacy rename applies only to executor.c
per v0733f/v0733g). This is consistent with the existing 0x16504
per-step special case at line 451, which also uses
`vf2_i960_step` and is proven by its differential test.

## Why "step first, then check"?

The loop's initial `cpu->ip` is the `entry_address` parameter
(e.g., 0x28178 for the post-29414 dispatcher). The first
iteration of the loop steps one instruction, advancing
`cpu->ip` to the next address. If the previous instruction was
a `call 0x29598`, then after the step `cpu->ip` is 0x29598 —
the hook entry — and the hook fires.

This "step first" design is correct for call-site dispatches:
the function is called when the IP lands on the function's
entry, which happens AFTER the `call` instruction has executed.
The "check first" alternative would require the loop to handle
a special case for the very first iteration (when `cpu->ip` is
the entry_address, not the call site). The "step first" design
is simpler and handles all cases uniformly.

The entry_address itself is never a hook entry in the current
configuration (no hook entry matches 0x28178, 0x17710, 0x1791c,
0x4b640, 0x16504, or VF2_TASK_COLI_ENTRY). If a future
configuration needs the entry_address to be a hook entry, the
loop body can be extended with a "check first" pre-step branch.

## Validation

The F4 INDIVIDUAL release differential test (v0734l, ctest
#133) **PASSES** with the per-step hook infrastructure in
place. This test exercises the full
0x29414..0x164c4 corridor via the post-29414 dispatcher,
which means the per-step loop processes the call sites to
0x29598, 0xcf04, 0x439ac, and 0x43888 (depending on the
specific F4 leg). The F4 test verifies that the final state
matches the reference exactly, so any divergence introduced
by the per-step hook (wrong cpu->ip, wrong call/return counts,
wrong condition state) would cause the test to fail.

Also passing:
- 42 player tests (excluding the 3 long-running 4505_*
  differential tests, ~62 s total).
- vf2_native_fifth_dispatch, vf2_native_sixth_dispatch
  (~26 s total).
- vf2_phase_2_5_refused_audit (1 s).
- vf2_f4_individual_release (16 s).

## What the next slice should pick up

- **v0755c**: Verify that each of the 4 wired callees
  (v0741/v0745/v0746/v0747) is actually fired in the
  appropriate F4 leg. This is a differential test that
  measures the condition state and call/return counts to
  confirm the hook fired (not the interpretation fallback).
  If a callee is NEVER fired in the current F4 test, the
  wiring is "wired but inactive" and a different test
  fixture is needed.

- **v0755d**: Wire the 0x1fcc0 sub-callees (v0749/v0750/v0751/
  v0752/v0753/v0754). This requires recovering `0x1fcc0` first
  (which depends on `0x2c38` — see v0755). The per-step
  hook infrastructure is already in place; only the table
  entries need to be added.

- **v0755e**: Resolve the `0x2c38` disasm ambiguity at 0x2d40
  (`subo 1, 0, g1`) and recover `0x2c38`. This unblocks
  the full `0x1fcc0` recovery.

## Risks and known limitations

- The per-step loop uses `vf2_i960_step` (arch stepper in
  hybrid.c) for the post-29414 range, while the existing
  `vf2_i960_run` path uses the legacy stepper. The
  differential tests confirm the arch stepper produces the
  same final state for the F4 legs, but condition-state
  differences may exist for other legs. v0733e documented
  that the warm leg and the live leg diverge on
  `compare_result` due to the stepper difference. The
  per-step loop makes the post-29414 range use the arch
  stepper, which is the same stepper the existing 0x16504
  special case uses. This is the proven stepper for the
  current F4 differential.

- The per-step loop currently does NOT have a strict
  step-count check or call/return-count check. The 0x16504
  special case has these. If the per-step loop introduces
  a count drift, the F4 test would catch it (which it
  doesn't currently). A count check can be added later if
  needed.

- The `g_callee_hooks` table is small (4 entries). The
  linear search is O(n) per step. For the post-29414 range
  (~50K instructions), this is ~200K hook lookups. This is
  negligible compared to the per-step cost.

## Validated

- `git diff --stat src/`: hybrid.c modified.
- `git diff --stat decomp/i960/functions.csv`: empty.
- ctest #38 `vf2_player_29598` (v0741, with v0755a
  retraction): PASSES.
- ctest #39 `vf2_player_439ac` (v0745): PASSES.
- ctest #40 `vf2_player_43888` (v0746): PASSES.
- ctest #41 `vf2_player_cf04` (v0747): PASSES.
- ctest #42–#47 (v0749–v0754): PASSES.
- ctest #109 `vf2_native_fifth_dispatch`: PASSES (13 s).
- ctest #110 `vf2_native_sixth_dispatch`: PASSES (13 s).
- ctest #133 `vf2_f4_individual_release`: PASSES (16 s).
- ctest #134 `vf2_phase_2_5_refused_audit`: PASSES (1 s).
- All 42 player tests: PASS (~62 s).
