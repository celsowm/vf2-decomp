# v0755a: v0741 path D retracted — clean refusal (no partial simulation)

## Summary

v0741 path D previously applied a partial simulation: the setbit
at 0x500068 and `cpu->g0 = 0x00ad231f`, then refused with
`cpu->ip = 0x295e8`. This was incompatible with the per-step
hook design (v0755b): setting `cpu->ip` past the function's body
would cause the hook to skip the 3 sub-calls and the `mov 0, g1`
between the entry and the ret. Path D now refuses **cleanly**
with `cpu->ip` unchanged at 0x29598 and no side-effects applied.

This is a backward-incompatible change to a previously-merged
recovery (v0741, ctest #38). The change is small and
self-contained: one function body, one test assertion block.

## What changed

### `src/recovered/hybrid.c:32515-32533` (the v0741 path D block)

**Before** (v0741, partial simulation):
```c
/* Path D: g1 == 15, setbit + 3 sub-calls.
 * Sub-calls (0xcf04, 0x439ac, 0x43888) are not yet recovered;
 * refuse this path so the dispatcher falls back to
 * hybrid_execute_interpreted_task. */
{
    uint32_t r15 = 0u;
    status = vf2_model2a_read_u32(
        machine, UINT32_C(0x00500068), &r15
    );
    if (status != VF2_OK) {
        return status;
    }
    r15 |= UINT32_C(1) << 20u;
    status = vf2_model2a_write_u32(
        machine, UINT32_C(0x00500068), r15
    );
    if (status != VF2_OK) {
        return status;
    }
    cpu->registers[VF2_I960_G0_REGISTER + 0u] =
        UINT32_C(0x00ad231f);
    cpu->ip = UINT32_C(0x000295e8);
    cpu->executed_instructions += UINT64_C(8);
}
return VF2_ERROR_UNSUPPORTED;
```

**After** (v0755a, clean refusal):
```c
/* Path D: g1 == 15, setbit + 3 sub-calls + mov 0, g1.
 * The 3 sub-calls (0xcf04, 0x439ac, 0x43888) ARE recovered
 * (v0747/v0745/v0746) but expanding path D to inline them
 * would require frame-push/pop plumbing inside this
 * function. Instead, we refuse cleanly with cpu->ip
 * UNCHANGED so the per-step hook (v0755) can step one
 * instruction forward and fall back to interpretation
 * (which will execute the setbit, the 3 sub-calls, the
 * mov 0, g1, and the ret). The function MUST NOT partially
 * simulate path D — that would diverge from the original
 * i960's state at the entry. See
 * per_step_hook_boundary_v0755.md. */
return VF2_ERROR_UNSUPPORTED;
```

### `tests/recovered/test_player_29598_native.c:119-148`

**Before**: the test primed 0x500068, called the function, then
asserted the setbit had run (`(r15_after & (1 << 20)) != 0`),
g0 had been set to 0x00ad231f.

**After**: the test asserts the OPPOSITE — cpu->ip is unchanged
at 0x29598, g0 is unchanged, 0x500068 is unchanged. The setbit
will be applied by the per-step hook's interpretation fallback,
not by this function.

### `decomp/i960/notes/fa_player_29598_recovered_v0741.md`

Added a banner at the top with the retraction notice and pointer
to `per_step_hook_boundary_v0755.md` for the design rationale.

## Why this is the right fix

The per-step hook design requires a "fully-handle OR cleanly-refuse"
discipline. The hook calls a function when `cpu->ip` lands on the
function's entry. The function either:
- Fully handles the call (returns `VF2_OK`, advances `cpu->ip` past
  the function's body).
- Refuses cleanly (returns `VF2_ERROR_UNSUPPORTED`, leaves `cpu->ip`
  at the entry, applies NO side-effects).

Path D's old partial-simulation pattern was a third option that
broke the discipline: it advanced `cpu->ip` past the function's
body while leaving the sub-calls and the `mov 0, g1` un-executed.
The per-step hook would then continue from the post-body IP,
silently skipping those instructions — diverging from the
original i960.

The clean-refusal discipline is consistent with how the per-step
hook is designed: a refused path means "I can't handle this
function, please interpret it step-by-step from the entry." The
per-step loop steps one instruction forward, re-checks the hook
table (no match — the new IP is `0x2959c`, not `0x29598`), and
continues interpretation. Interpretation executes the setbit,
the 3 sub-calls, the `mov 0, g1`, and the ret in the original
i960's order.

## Why not inline the 3 sub-calls (alternative)?

Inlining the 3 sub-calls into path D would have required:
- Push a frame for the first sub-call (0xcf04): enter procedure
  with `entry_ip = 0xcf04`, `ret_ip = 0x295d4` (post-call IP).
- Call the recovered sub-call function.
- Pop the frame.
- Push a frame for the second sub-call (0x439ac): enter procedure
  with `entry_ip = 0x439ac`, `ret_ip = 0x295dc`.
- Call, pop.
- Push a frame for the third sub-call (0x43888): enter procedure
  with `entry_ip = 0x43888`, `ret_ip = 0x295e0`.
- Call, pop.
- Handle the `mov 0, g1` at 0x295e4.
- Handle the ret at 0x295e8.

This would have been a substantial expansion of v0741 path D, and
the resulting function would be tightly coupled to the per-call
contract of the dispatcher (frame-push/pop pattern, IP-tracking
conventions). The clean-refusal approach is much smaller and
preserves the v0741 function's role as a "stand-alone execute
hook" — its public API doesn't depend on dispatcher internals.

## Validated

- ctest #38 (vf2_player_29598) PASSES with the new clean-refusal
  assertions: cpu->ip == 0x29598, g0 unchanged, 0x500068 unchanged.
- All 4 paths in the function are still accounted for:
  - Path A (g0 bit 4 clear): fully recovered, VF2_OK.
  - Path B (bbc taken): fully recovered, VF2_OK.
  - Path C (bbc not taken, g1 != 15): fully recovered, VF2_OK.
  - Path D (bbc not taken, g1 == 15): cleanly refused, VF2_ERROR_UNSUPPORTED.
- v0755b (per-step hook) is now unblocked — the function's
  refusal contract is consistent with the hook's design.
