# v0744: 0x29598 wiring — boundary investigation, NOT yet wired

**Slice 6 of the `advance_plan_v0735.md` chain (continuation of v0741).**
This is a **boundary investigation**, not a wiring. The v0741
recovery is unit-tested and passes (ctest #38, 0.03 s), but it is
not yet active in the running code. The dispatcher chain in
`hybrid_execute_player_post_29414` still runs 0x29598 via
`hybrid_execute_interpreted_until(0x28178, 0x14400)`, which uses
`vf2_i960_run` to interpret the entire range including 0x29598.

**No `src/` change. No `functions.csv` change.**

## The wiring challenge

The 0x29598 function lives inside the `0x28178..0x14400` range
that the post-29414 dispatcher interprets as a single segment.
There is no per-IP hook in `hybrid_execute_interpreted_until`
(line 436) that would let a recovered function intercept a
callee inside the interpreted range. The function uses
`vf2_i960_run` (line 473) which has only `stop_address` and
`max_steps` — not a per-step callback.

The natural place to wire 0x29598 is one of:

1. **Per-step loop in `hybrid_execute_interpreted_until`.** Add a
   while loop similar to the existing special case at line 456
   for the `0x16504 -> 0x14418` segment. The loop would:
   - `vf2_i960_step(cpu, machine, NULL)` per iteration
   - Check `cpu->ip == 0x00029598` after each step
   - If so, call `vf2_hybrid_player_29598_execute` and on
     `VF2_ERROR_UNSUPPORTED` fall back to a single step and retry
   - This is invasive — it changes the generic dispatcher, and
     every other recovered function would need a similar hook.

2. **Per-IP check at the start of `hybrid_execute_player_post_29414`.**
   If `cpu->ip == 0x00029598`, call the recovered function. But
   the post-29414 is called with `cpu->ip == 0x00028178` (set by
   `hybrid_execute_player_29414`'s return), so this check
   **never fires** in the current chain. Wiring this would
   require a different entry point that the i960 program doesn't
   naturally use.

3. **Split the 0x28178..0x14400 segment into smaller pieces.**
   Run `hybrid_execute_interpreted_until(0x28178, 0x29598)` to
   stop just before 0x29598, then check the next IP, then handle
   0x29598 explicitly, then continue with the rest. This breaks
   the i960 program's call structure (the call to 0x29598 might
   be conditional and skipped entirely) and would require the
   dispatcher to know that 0x29598 is "in the range" of this
   segment, which is fragile.

## What's actually needed

A **per-step hook** in the hybrid dispatcher that lets recovered
functions be called when the IP lands on their entry. This is a
generic capability that all future callee recoveries will need,
not just 0x29598. The natural design is:

```c
typedef vf2_status (*vf2_recovered_callee_hook)(
    vf2_model2a *machine, vf2_i960_cpu *cpu);

/* Called from the per-step loop; returns VF2_OK if the hook
 * handled the call, VF2_ERROR_UNSUPPORTED to fall through to
 * the next hook, or any other status to abort. */
```

A small table of `cpu->ip` -> `hook_fn` mappings, checked after
each `vf2_i960_step`. The 0x29598 function would be the first
entry. This is the right design for Phase 3 onward, but it's a
substantial refactor of the hybrid dispatcher.

## Why this slice is a boundary note, not a recovery

The v0741 unit test (ctest #38) proves the recovered function is
correct: paths A/B/C succeed, path D is refused, the IP lands
on 0x295e8, the executed_instructions count matches. The
**only** thing missing is the dispatcher integration.

The next session should pick up this work. The v0744 entry is
the natural next slice after v0741.

## Validated

- `git diff --stat src/`: empty.
- `git diff --stat decomp/i960/functions.csv`: empty.
- ctest #38 `vf2_player_29598` (v0741): PASSES.
- ctest #125 `vf2_phase_2_5_refused_audit` (v0736): PASSES.
- ctest #124 `vf2_f4_individual_release` (v0734l): PASSES.
- The recovered function is well-tested; the gap is dispatcher
  integration, not recovery correctness.

## What the next slice should pick up

- **v0745 candidate A**: Implement the per-step hook in
  `hybrid_execute_interpreted_until` and add 0x29598 as the
  first entry. Verify via a native differential test that the
  full 0x29414..0x164c4 corridor still FULL MATCHES.
- **v0745 candidate B**: Recover the 0x29414 callee #2
  (`0xcf04`, 184 B) or callee #3 (`0x439ac`, 80 B) per the
  v0737 boundary. Each is a standalone execute hook; a future
  dispatcher can add it to the per-step table once the hook
  infrastructure exists.
- **v0745 candidate C**: Recover the `0x43888`
  `VF2_SELECTOR2_QUEUE_ENTRY` callee (200 B, the largest of
  the 4 callees). Per v0737, this is a queue entry handler
  that needs structural recovery.
