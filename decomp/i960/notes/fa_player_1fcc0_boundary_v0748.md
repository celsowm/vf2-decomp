# v0748: 0x1fcc0 (`display_profile_apply`) — boundary investigation, NOT yet recovered

**Slice 7 of the advance plan execution chain.** This is a
**boundary investigation** for the next major callee to recover.
All four callees of `0x29414` (v0737) are now recovered (v0741,
v0745, v0746, v0747). The refused sub-call in v0747 is to
`0x1fcc0` (`display_profile_apply`), a 548 B function that
unblocks the rest of the post-29414 chain.

**No `src/` change. No `functions.csv` change.**

## Function shape

```
display_profile_apply: address=0x0001fcc0 end=0x0001fee4 blocks=15
```

548 B, 15 blocks, no indirect branches. The function does:
1. Read fighter bases from `0x500804` and `0x500808`.
2. Check fighter types at `+0x1b1` for a 2+1 / 1+2 combination
   to decide a profile mode (0x0c / others).
3. Dispatch on `0x500064` (a state byte) and `0x50004c`.
4. Write float constants to `0x50a000`/`0x50a004` and `0x50a124`/`0x50a128`.
5. Call `0x1ff0c` (`display_profile_mode_constants`, 240 B,
   5 blocks, sub-callee #1).
6. Read mode byte from `0x6eeae[r4*2]` and write to `0x501018` /
   `0x500170` / `0x501098` / `0x501020` / `0x501022`.
7. Call `0x1fffc` (sub-callee #2).
8. Load g0/g1 from offset, call `0x4b410` (sub-callee #3).
9. Clear 0x50a014..0x50a026 (8 zero-writes).
10. Call `0x2eab8` (sub-callee #4) and `0x11704` (sub-callee #5).
11. Ret.

## Sub-callees to recover first

To recover 0x1fcc0, the 5 sub-callees must be recovered too (or
refused at the call site). Each one is roughly tractable:

| callee | size | blocks | known name | scope |
|---|---|---|---|---|
| 0x1fee4 | 36 B | 5 | (init 27 4-byte float values) | trivial |
| 0x1ff0c | 240 B | 5 | `display_profile_mode_constants` | 3 paths, 1 sub-call to 0x1fee4 |
| 0x1fffc | ? | ? | (likely a vertex/object builder) | unknown |
| 0x4b410 | ? | ? | (a generic player/coli helper) | unknown |
| 0x2eab8 | ? | ? | (TGP / geometry helper?) | unknown |
| 0x11704 | ? | ? | (another generic helper) | unknown |

A full v0748 recovery would require 5-6 separate slices (one per
callee), each with its own ctest entry. Estimated 1-2 weeks of
focused work.

## The right next slice: per-step hook infrastructure

Even after all 5 callees are recovered, the dispatcher chain in
`hybrid_execute_player_post_29414` (and its successors) uses
`hybrid_execute_interpreted_until(0x28178, 0x14400)` which runs
the entire range via `vf2_i960_run` with only `stop_address` and
`max_steps`. None of the recovered callees (v0741/v0745/v0746/v0747)
are actually called from the running code.

To make them active, a **per-IP hook table** in
`hybrid_execute_interpreted_until` is needed. The design is:

```c
/* per-IP hook table */
typedef vf2_status (*vf2_hybrid_hook_fn)(
    vf2_model2a *machine, vf2_i960_cpu *cpu);

typedef struct {
    uint32_t ip;
    vf2_hybrid_hook_fn fn;
} vf2_hybrid_hook_entry;

/* per-step loop in hybrid_execute_interpreted_until */
while (cpu->ip != stop_address && steps < limit) {
    status = vf2_i960_step(cpu, machine, NULL);
    if (status == VF2_OK) {
        /* check hook table for cpu->ip */
        for (size_t i = 0; i < hook_count; ++i) {
            if (cpu->ip == hooks[i].ip) {
                vf2_status hook_status = hooks[i].fn(machine, cpu);
                if (hook_status == VF2_ERROR_UNSUPPORTED) {
                    /* let the next step interpret naturally */
                    break;
                }
                if (hook_status != VF2_OK) {
                    return hook_status;
                }
                /* hook succeeded; continue the loop */
                break;
            }
        }
    }
    ++steps;
}
```

This pattern is already used for the special case at line 456 of
`hybrid.c` (the 0x16504 -> 0x14418 segment with a hardcoded 1690
step limit and 16/17 call/return count). The generalisation
requires:
1. Define the hook table type.
2. Initialise it with the 4 known callees.
3. Modify `hybrid_execute_interpreted_until` to use the per-step
   loop pattern for any range that contains a hooked IP.
4. Add a native differential ctest that proves the FULL MATCH at
   the end of the post-29414 chain.

## What's in this slice (v0748) and what's NOT

- This slice is **status only** — no code change.
- The full v0748+ recovery (5 callees + per-step hook) is a
  multi-week effort, the largest remaining bulk in Phase 3.

## What the next session should pick up

- **v0749 candidate A**: implement the per-step hook infrastructure.
  Then 0x29598, 0x439ac, 0x43888, 0xcf04 all become active in the
  running code, FULL MATCH still holds at 0x164c4.
- **v0749 candidate B**: recover `0x1fee4` (the trivial 36-byte
  init function). It's a clean starting point for the 0x1fcc0
  chain. The ctest would verify the 27 4-byte float writes.
- **v0749 candidate C**: recover `0x1ff0c` (the mode-constants
  function). 240 B, 5 blocks, 3 paths. The 0x1fee4 sub-call can
  be refused (the function's main effect is parameter loading,
  which is safe to redo).
- **v0749 candidate D**: any other small function in the post-29414
  corridor that's tractable and not in the 0x1fcc0 chain.

## Validated

- `git diff --stat src/`: empty.
- `git diff --stat decomp/i960/functions.csv`: empty.
- ctest #38, #39, #40, #41 (v0741, v0745, v0746, v0747): all PASS.
- ctest #128 `vf2_phase_2_5_refused_audit`: PASS.
- All 4 callees of 0x29414 have unit-testable C recoveries; the
  gap is dispatcher integration, not recovery correctness.
